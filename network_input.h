#pragma once

#include "network_input_replica_config.h"

#include "scene/main/node.h"

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

	NetworkInput();
};
