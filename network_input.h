#pragma once

#include "network_input_replica_config.h"

#include "scene/main/node.h"

class NetworkInput : public Node {
	GDCLASS(NetworkInput, Node)

private:
	Ref<NetworkInputReplicaConfig> replica_config;

protected:
	static void _bind_methods();

public:
	void set_replica_config(Ref<NetworkInputReplicaConfig> p_config);
	Ref<NetworkInputReplicaConfig> get_replica_config();
	NetworkInputReplicaConfig *get_replica_config_ptr() const;

	PackedStringArray get_configuration_warnings() const override;
};
