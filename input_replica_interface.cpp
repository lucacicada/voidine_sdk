#include "input_replica_interface.h"
#include "core/io/marshalls.h"
#include "network_input.h"
#include "rollback_multiplayer.h"

Error InputReplicaInterface::add_input(NetworkInput *p_input) {
	ERR_FAIL_COND_V(!p_input, ERR_INVALID_PARAMETER);
	inputs.insert(p_input->get_instance_id(), InputState());
	return OK;
}

Error InputReplicaInterface::remove_input(NetworkInput *p_input) {
	ERR_FAIL_COND_V(!p_input, ERR_INVALID_PARAMETER);
	inputs.erase(p_input->get_instance_id());
	return OK;
}

void InputReplicaInterface::process_inputs() {
	// TODO: sample inputs here if enabled?
	for (KeyValue<ObjectID, InputState> &E : inputs) {
		const ObjectID &oid = E.key;
		NetworkInput *input = oid.is_valid() ? ObjectDB::get_instance<NetworkInput>(oid) : nullptr;
		if (!input) {
			continue;
		}

		if (input->is_multiplayer_authority()) { // only gather local authority
			input->gather();
		} else if (multiplayer->is_server()) {
			RingBuffer<InputFrame> &buffer = E.value.input_buffer;
			if (buffer.data_left() > 0) {
				InputFrame frame = buffer.read();

				Ref<NetworkInputReplicaConfig> replica_config = input->get_replica_config();
				if (replica_config.is_null()) {
					continue;
				}
				const TypedArray<NodePath> props = replica_config->get_properties();
				for (const NodePath &prop : props) {
					if (!frame.properties.has(prop)) {
						continue;
					}
					const Variant &value = frame.properties[prop];
					input->set_indexed(prop.get_names(), value);
				}
			}
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
	if (replica_config.is_null()) {
		return OK; // nothing to send
	}

	const TypedArray<NodePath> props = replica_config->get_properties();
	if (props.is_empty()) {
		return OK; // nothing to send
	}

	Vector<const InputFrame *> frames;
	input->get_last_input_frames_asc(4, frames); // TODO: configurable?
	if (frames.is_empty()) {
		return OK; // nothing to send
	}

	Vector<Variant> state_vars;
	Vector<const Variant *> varp;
	state_vars.resize(frames.size() * (props.size() + 1)); // +1 for tick
	varp.resize(state_vars.size());

	int index = 0;
	for (int i = 0; i < frames.size(); i++) {
		const InputFrame *frame = frames[i];

		state_vars.write[index] = uint64_t(frame->frame_id);
		varp.write[index] = &state_vars.write[index];
		index++;

		for (const NodePath &prop : props) {
			const Variant *value = frame->properties.getptr(prop);
			ERR_FAIL_COND_V_MSG(value == nullptr, ERR_INVALID_PARAMETER, "Input property missing in input frame.");
			state_vars.write[index] = *value;
			varp.write[index] = &state_vars.write[index];
			index++;
		}
	}

	int size;
	Error err = MultiplayerAPI::encode_and_compress_variants(varp.ptrw(), varp.size(), nullptr, size);
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

void InputReplicaInterface::_process_inputs(int p_from, const uint8_t *p_packet, int p_packet_len) {
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
		if (input_state->input_buffer.space_left() == 0) {
			break; // no more space
		}

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
		input_state->input_buffer.write(frame);
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
