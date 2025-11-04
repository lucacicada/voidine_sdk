#pragma once

#include "core/object/ref_counted.h"
#include "network_input.h"

class RollbackMultiplayer;
class NetworkInput;

class Replica : public RefCounted {
	GDCLASS(Replica, RefCounted);

private:
	struct InputPropertyCache {
		NodePath property;
		Variant::Type type;
	};
	struct InputConfigCache {
		Vector<InputPropertyCache> properties;
	};

	RollbackMultiplayer *multiplayer = nullptr;
	HashMap<ObjectID, InputConfigCache> inputs;

	Error _send_inputs(const NetworkInput::InputFrame **p_frames, int p_frame_count, const InputConfigCache &p_config);

	Vector<uint8_t> packet_cache;
	Error _send_raw(const uint8_t *p_buffer, int p_size, int p_peer, bool p_reliable);

public:
	void process_inputs(int p_from, const uint8_t *p_packet, int p_packet_len);

	Error add_input(NetworkInput *p_input);
	Error remove_input(NetworkInput *p_input);

	void gather_inputs();

	Replica(RollbackMultiplayer *p_multiplayer) {
		multiplayer = p_multiplayer;
	}
};
