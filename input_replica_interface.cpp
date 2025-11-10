#include "input_replica_interface.h"
#include "core/io/marshalls.h"
#include "network_input.h"
#include "rollback_multiplayer.h"

void InputReplicaInterface::_input_ready(const ObjectID &p_oid) {
	NetworkInput *input = p_oid.is_valid() ? ObjectDB::get_instance<NetworkInput>(p_oid) : nullptr;
	ERR_FAIL_NULL(input); // should never happen

	// on the server track all inputs
	// on the client, only add inputs that are authoritative (local)
	if (multiplayer->is_server() || input->is_multiplayer_authority()) {
		inputs.insert(p_oid, InputState());

		int peer_id = input->get_multiplayer_authority();

		// TODO: NetworkInput should target the root node that is responsible to set the authority
		// ERR_FAIL_COND_MSG(peer_inputs.has(peer_id), vformat("Input already registered for peer %d.", peer_id));

		peer_inputs.insert(peer_id, p_oid); // override for now, this wont cause issue as long as NetworkInput unregister itself
	}
}

Error InputReplicaInterface::add_input(Object *p_obj, Variant p_config) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node || p_config.get_type() != Variant::OBJECT, ERR_INVALID_PARAMETER);
	NetworkInput *input = Object::cast_to<NetworkInput>(p_config.get_validated_object());
	ERR_FAIL_NULL_V(input, ERR_INVALID_PARAMETER);

	const ObjectID oid = input->get_instance_id();

	// delay the initialization until the node is ready
	// this will give a chance for the input to configure it's own authority
	if (node->is_ready()) {
		_input_ready(oid);
	} else {
		node->connect(SceneStringName(ready), callable_mp(this, &InputReplicaInterface::_input_ready).bind(oid), Node::CONNECT_ONE_SHOT);
	}

	return OK;
}

Error InputReplicaInterface::remove_input(Object *p_obj, Variant p_config) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node || p_config.get_type() != Variant::OBJECT, ERR_INVALID_PARAMETER);
	NetworkInput *input = Object::cast_to<NetworkInput>(p_config.get_validated_object());
	ERR_FAIL_NULL_V(input, ERR_INVALID_PARAMETER);

	const ObjectID oid = input->get_instance_id();

	inputs.erase(oid);
	for (const KeyValue<int, ObjectID> &E : peer_inputs) {
		if (E.value == oid) {
			peer_inputs.erase(E.key);
			break;
		}
	}

	return OK;
}

void InputReplicaInterface::gather_inputs() {
	// TODO: sample inputs here if enabled?

	// to sample inputs we must collect them during idle (process) frame
	// int samples = 0;
	// _process() {
	//  _gather();
	// 	samples++;
	//  for (prop in props) {
	//    cached_props[prop] += current_value[prop]
	//  }
	// }

	for (KeyValue<ObjectID, InputState> &E : inputs) {
		const ObjectID &oid = E.key;
		NetworkInput *input = oid.is_valid() ? ObjectDB::get_instance<NetworkInput>(oid) : nullptr;
		if (!input) {
			continue;
		}

		if (input->is_multiplayer_authority()) { // only gather local authority
			input->gather();
		} else if (multiplayer->is_server()) {
			input->replay(); // replay from buffer on server
		}
	}

	// only the client can send
	if (multiplayer->get_rollback_state() == RollbackMultiplayer::ROLLBACK_STATE_CLIENT) {
		_send_local_inputs();
	}
}

Error InputReplicaInterface::_send_local_inputs() {
	NetworkInput *input = nullptr;
	for (const KeyValue<ObjectID, InputState> &E : inputs) {
		const ObjectID &oid = E.key;
		NetworkInput *i = oid.is_valid() ? ObjectDB::get_instance<NetworkInput>(oid) : nullptr;
		if (i && i->is_multiplayer_authority()) {
			input = i;
			break; // HACK: only send the first input we find, ideally multiple inputs should be supported
		}
	}

	if (!input) {
		return OK; // nothing to send
	}

	Ref<NetworkInputReplicaConfig> replica_config = input->get_replica_config();
	ERR_FAIL_COND_V(replica_config.is_null(), ERR_UNCONFIGURED);

	const TypedArray<NodePath> props = replica_config->get_properties();
	ERR_FAIL_COND_V(props.is_empty(), ERR_UNCONFIGURED);

	Vector<InputFrame> frames;
	Error err = input->copy_buffer(frames, 4);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Unable to copy input buffer.");

	if (frames.is_empty()) {
		return OK; // nothing to send
	}

	Vector<Variant> state_vars;
	Vector<const Variant *> varp;

	state_vars.resize(frames.size() * (props.size() + 1)); // +1 for frame_id
	varp.resize(state_vars.size());

	for (int i = 0; i < frames.size(); i++) {
		int offset = i * (props.size() + 1);

		const InputFrame &frame = frames[i];
		state_vars.write[offset] = frame.frame_id;
		varp.write[offset] = &state_vars[offset];

		for (int j = 0; j < props.size(); j++) {
			const NodePath &prop = props[j];
			ERR_FAIL_COND_V_MSG(!frame.properties.has(prop), ERR_DOES_NOT_EXIST, vformat("Property '%s' not found in input frame.", prop));

			state_vars.write[offset + 1 + j] = frame.properties[prop];
			varp.write[offset + 1 + j] = &state_vars[offset + 1 + j];
		}
	}

	int size;
	err = MultiplayerAPI::encode_and_compress_variants(varp.ptrw(), varp.size(), nullptr, size);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Unable to encode input buffer.");

	if (packet_cache.size() < 2 + size) {
		packet_cache.resize(2 + size);
	}

	uint8_t *ptr = packet_cache.ptrw();
	ptr[0] = SceneMultiplayer::NETWORK_COMMAND_RAW | 1 << SceneMultiplayer::CMD_FLAG_1_SHIFT;
	ptr[1] = uint8_t(frames.size());
	MultiplayerAPI::encode_and_compress_variants(varp.ptrw(), varp.size(), &ptr[2], size);

	return _send_raw(packet_cache.ptr(), (2 + size), 1, false); // send to server
}

void InputReplicaInterface::process_inputs(int p_from, const uint8_t *p_packet, int p_packet_len) {
	ERR_FAIL_COND(!multiplayer->is_server());

	ERR_FAIL_COND_MSG(p_from == 1, "Input packets should only come from peers.");
	ERR_FAIL_COND_MSG(p_packet_len < 2, "Invalid input packet received. Size too small.");

	const uint8_t frames_count = p_packet[1];
	ERR_FAIL_COND_MSG(frames_count == 0, "Input packet contains zero frames.");
	ERR_FAIL_COND_MSG(frames_count > 64, "Input packet contains too many frames.");

	// find the input owner
	NetworkInput *input = nullptr;
	InputState *input_state = nullptr;
	for (const KeyValue<ObjectID, InputState> &E : inputs) {
		input = E.key.is_valid() ? ObjectDB::get_instance<NetworkInput>(E.key) : nullptr;
		if (input && input->get_multiplayer_authority() == p_from) {
			input = input;
			input_state = &inputs[E.key];
			break;
		}
	}

	ERR_FAIL_COND_MSG(!input || !input_state, "Received input packet from unknown peer.");
	Ref<NetworkInputReplicaConfig> replica_config = input->get_replica_config();
	ERR_FAIL_COND_MSG(replica_config.is_null(), "Received input from peer with no configured replica.");
	const TypedArray<NodePath> props = replica_config->get_properties();
	ERR_FAIL_COND_MSG(props.is_empty(), "Received input from peer with no configured properties.");

	// ERR_FAIL_COND_MSG(input_state->input_buffer.space_left() < frames_count, "Not enough space in input buffer to store received input frames.");

	const int64_t prop_size = props.size();
	int varc = frames_count * (prop_size + 1); // +1 is tick

	ERR_FAIL_COND_MSG(varc <= 0 || varc > 1024, "Input packet contains too much data"); // 1024 hold over 100 props for 8 frames

	Vector<Variant> vars;
	vars.resize(varc);

	int consumed = 0;
	Error err = MultiplayerAPI::decode_and_decompress_variants(vars, p_packet + 2, p_packet_len - 2, consumed);
	ERR_FAIL_COND(err != OK);
	ERR_FAIL_COND(vars.size() != varc); // should not happen

	// read each frame
	for (int i = 0; i < frames_count; i++) {
		const int64_t base_idx = i * (prop_size + 1);

		InputFrame frame;
		frame.frame_id = uint64_t(vars[base_idx]);
		ERR_FAIL_COND_MSG(frame.frame_id == 0, "Received input frame with invalid tick 0.");

		if (input_state->last_aknownedged_input_id >= frame.frame_id) {
			continue; // already have this input
		}

		for (int j = 0; j < prop_size; j++) {
			const int64_t prop_idx = base_idx + 1 + j;
			ERR_FAIL_UNSIGNED_INDEX(prop_idx, vars.size());
			ERR_FAIL_INDEX(j, props.size());
			frame.properties.insert(props[j], vars[prop_idx]);
		}

		input->write_frame(frame);
		input_state->last_aknownedged_input_id = frame.frame_id;
	}
}

Error InputReplicaInterface::_send_raw(const uint8_t *p_buffer, int p_size, int p_peer, bool p_reliable) {
	ERR_FAIL_COND_V(!p_buffer || p_size < 1, ERR_INVALID_PARAMETER);

	Ref<MultiplayerPeer> peer = multiplayer->get_multiplayer_peer();
	ERR_FAIL_COND_V(peer.is_null(), ERR_UNCONFIGURED);
	peer->set_transfer_channel(0);
	peer->set_transfer_mode(p_reliable ? MultiplayerPeer::TRANSFER_MODE_RELIABLE : MultiplayerPeer::TRANSFER_MODE_UNRELIABLE);
	return multiplayer->send_command(p_peer, p_buffer, p_size);
}
