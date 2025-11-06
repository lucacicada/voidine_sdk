#include "input_replica_interface.h"
#include "core/io/marshalls.h"
#include "network_input.h"
#include "rollback_multiplayer.h"

Error InputReplicaInterface::add_input(NetworkInput *p_input) {
	ERR_FAIL_COND_V(!p_input, ERR_INVALID_PARAMETER);
	Ref<NetworkInputReplicaConfig> replica_config = p_input->get_replica_config();
	ERR_FAIL_COND_V(replica_config.is_null(), ERR_INVALID_PARAMETER);
	const TypedArray<NodePath> props = replica_config->get_properties();
	ERR_FAIL_COND_V(props.is_empty(), ERR_INVALID_PARAMETER);

	// take a cache, this is wrong if properties change at runtime
	// but it save the headache of constantly looking up types
	InputConfigCache config;
	for (const NodePath &prop : props) {
		bool valid = false;
		const Variant &v = p_input->get_indexed(prop.get_names(), &valid);
		ERR_FAIL_COND_V(!valid, ERR_INVALID_PARAMETER);
		const Variant::Type v_type = v.get_type();
		config.properties.push_back({ prop, v_type });
	}

	const ObjectID oid = p_input->get_instance_id();
	inputs.insert(oid, config);
	return OK;
}

Error InputReplicaInterface::remove_input(NetworkInput *p_input) {
	ERR_FAIL_COND_V(!p_input, ERR_INVALID_PARAMETER);
	const ObjectID oid = p_input->get_instance_id();
	inputs.erase(oid);
	return OK;
}

void InputReplicaInterface::gather_inputs() {
	// TODO: sample inputs here if enabled?
	for (const KeyValue<ObjectID, InputConfigCache> &E : inputs) {
		const ObjectID &oid = E.key;
		NetworkInput *input = oid.is_valid() ? ObjectDB::get_instance<NetworkInput>(oid) : nullptr;
		if (input && input->is_multiplayer_authority()) { // only buffer local authority
			input->gather();
		}
	}

	// only the client can send
	if (multiplayer->get_rollback_state() != RollbackMultiplayer::ROLLBACK_STATE_CLIENT) {
		return;
	}

	// most of the time only 1 local input exists, but there could be more
	for (const KeyValue<ObjectID, InputConfigCache> &E : inputs) {
		const ObjectID &oid = E.key;
		const InputConfigCache &config = E.value;

		NetworkInput *input = oid.is_valid() ? ObjectDB::get_instance<NetworkInput>(oid) : nullptr;
		if (!input || !input->is_multiplayer_authority()) { // only submit my own inputs
			continue;
		}

		Vector<const InputFrame *> frames;
		input->get_last_input_frames_asc(4, frames); // TODO: configurable?
		if (frames.is_empty()) {
			continue;
		}

		_send_inputs(frames.ptrw(), frames.size(), config);
	}
}

Error InputReplicaInterface::_send_inputs(const InputFrame **p_frames, int p_frame_count, const InputConfigCache &p_config) {
	// pack frame count
	// for each frame pack
	// - frame tick
	// - list of properties

	Vector<Variant> state_vars;
	Vector<const Variant *> varp;
	state_vars.resize(p_frame_count * (p_config.properties.size() + 1)); // +1 for tick
	varp.resize(state_vars.size());

	int index = 0;
	for (int i = 0; i < p_frame_count; i++) {
		const InputFrame *frame = p_frames[i];

		state_vars.write[index] = uint64_t(frame->tick);
		varp.write[index] = &state_vars.write[index];
		index++;

		for (const InputPropertyCache &prop : p_config.properties) {
			const Variant *value = frame->properties.getptr(prop.property);
			ERR_FAIL_COND_V_MSG(value == nullptr, ERR_INVALID_PARAMETER, "Input property missing in input frame.");
			ERR_FAIL_COND_V_MSG(value->get_type() != prop.type, ERR_INVALID_PARAMETER, "Input property type mismatch.");

			state_vars.write[index] = *value;
			varp.write[index] = &state_vars.write[index];
			index++;
		}
	}

	int size;
	Error err = MultiplayerAPI::encode_and_compress_variants(varp.ptrw(), varp.size(), nullptr, size);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Unable to encode input buffer.");

	// TODO: report("Encoded input packet size: " + itos(size) + ":" + itos(varp.size()) + " variants.");

	if (packet_cache.size() < 2 + size) {
		packet_cache.resize(2 + size);
	}

	uint8_t *ptr = packet_cache.ptrw();
	ptr[0] = SceneMultiplayer::NETWORK_COMMAND_RAW | 1 << SceneMultiplayer::CMD_FLAG_1_SHIFT;
	ptr[1] = uint8_t(p_frame_count);
	MultiplayerAPI::encode_and_compress_variants(varp.ptrw(), varp.size(), &ptr[2], size);

	return _send_raw(packet_cache.ptr(), (2 + size), 1, false); // send to server
}

void InputReplicaInterface::process_inputs(int p_from, const uint8_t *p_packet, int p_packet_len) {
	ERR_FAIL_COND(!multiplayer->is_server());

	ERR_FAIL_COND_MSG(p_from == 1, "Input packets should only come from peers.");
	ERR_FAIL_COND_MSG(p_packet_len < 2, "Invalid input packet received. Size too small.");

	const uint8_t frames_count = p_packet[1];
	ERR_FAIL_COND_MSG(frames_count == 0, "Input packet contains zero frames.");

	// count the expected number of vars

	// find the input owner
	NetworkInput *input = nullptr;
	InputConfigCache *config = nullptr;
	for (const KeyValue<ObjectID, InputConfigCache> &E : inputs) {
		input = E.key.is_valid() ? ObjectDB::get_instance<NetworkInput>(E.key) : nullptr;
		if (input && input->get_multiplayer_authority() == p_from) {
			config = &inputs[E.key];
			break;
		}
	}

	ERR_FAIL_COND_MSG(!input || !config, "Received input packet from unknown peer.");
	const int64_t prop_size = config->properties.size();
	int varc = frames_count * (prop_size + 1); // first v is tick

	ERR_FAIL_COND(varc <= 0 || varc > 1024); // 1024 hold over 100 props for 8 frames

	Vector<Variant> vars;
	vars.resize(varc);

	int consumed = 0;
	Error err = MultiplayerAPI::decode_and_decompress_variants(vars, p_packet + 2, p_packet_len - 2, consumed);
	ERR_FAIL_COND(err != OK);
	ERR_FAIL_COND(vars.size() != varc);

	// read each frame
	for (int i = 0; i < frames_count; i++) {
		InputFrame frame;
		const int64_t base_idx = i * (prop_size + 1);
		ERR_FAIL_UNSIGNED_INDEX(base_idx, vars.size());
		frame.tick = uint64_t(vars[base_idx]);
		ERR_FAIL_COND_MSG(frame.tick == 0, "Received input frame with invalid tick 0.");
		for (int j = 0; j < prop_size; j++) {
			const InputPropertyCache &prop = config->properties[j];
			const int64_t prop_idx = base_idx + 1 + j;
			ERR_FAIL_UNSIGNED_INDEX(prop_idx, vars.size());
			frame.properties.insert(prop.property, vars[prop_idx]);
		}
		input->push_frame(frame);
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
