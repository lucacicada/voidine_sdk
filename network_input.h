#pragma once

#include "network_input_replica_config.h"

#include "scene/main/node.h"

// class InputBuffer {
// private:
// 	friend class NetworkInput; // NetworkInput has direct access to the buffer

// 	TightLocalVector<InputFrame> buffer;

// public:
// 	uint32_t get_capacity() const {
// 		return buffer.size();
// 	}

// 	void set_capacity(uint32_t p_capacity) {
// 		ERR_FAIL_COND(p_capacity == 0, "Input buffer capacity must be greater than zero.");
// 		buffer.resize(p_capacity);
// 	}

// 	InputBuffer() {
// 		buffer.resize(1); // ensure the buffer is never empty
// 	}
// };

class NetworkInput : public Node {
	GDCLASS(NetworkInput, Node)

private:
	struct InputFrame {
		double time = 0;
		double delta = 0;
		uint64_t tick = 0;
		HashMap<NodePath, Variant> properties;
	};

	Ref<NetworkInputReplicaConfig> replica_config;
	TightLocalVector<InputFrame> input_buffer;

protected:
	static void _bind_methods();

public:
	GDVIRTUAL0(_gather); // for compatibility

	void set_replica_config(Ref<NetworkInputReplicaConfig> p_config);
	Ref<NetworkInputReplicaConfig> get_replica_config();
	NetworkInputReplicaConfig *get_replica_config_ptr() const;

	PackedStringArray get_configuration_warnings() const override;

	void buffer();
	void clear_buffer();
	void replay_input(uint64_t p_tick);
	bool has_input(uint64_t p_tick) const {
		if (input_buffer.size() > 0) {
			const int32_t idx = p_tick % input_buffer.size();
			return input_buffer[idx].tick == p_tick;
		} else {
			return false;
		}
	}

	NetworkInput();
};
