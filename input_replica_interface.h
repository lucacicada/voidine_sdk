#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "network_input.h"

class RollbackMultiplayer;
class NetworkInput;

// Interface for replicating input from clients to server
class InputReplicaInterface : public RefCounted {
	GDCLASS(InputReplicaInterface, RefCounted);

private:
	struct InputState {
		uint64_t last_aknownedged_input_id = 0;
	};

	HashMap<ObjectID, InputState> inputs;
	RollbackMultiplayer *multiplayer = nullptr;

	Error _send_local_inputs();

	Vector<uint8_t> packet_cache;
	Error _send_raw(const uint8_t *p_buffer, int p_size, int p_peer, bool p_reliable);

public:
	Error add_input(NetworkInput *p_input);
	Error remove_input(NetworkInput *p_input);

	void process_inputs();
	void process_inputs(int p_from, const uint8_t *p_packet, int p_packet_len);

	InputReplicaInterface(RollbackMultiplayer *p_multiplayer) {
		multiplayer = p_multiplayer;
	}
};
