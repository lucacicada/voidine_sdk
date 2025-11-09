#pragma once

#include "network_replica_config.h"
#include "scene/main/node.h"

// NetworkEntity is ok
// NetworkObject is misleading...
class NetworkActor : public Node {
	GDCLASS(NetworkActor, Node)

private:
	Ref<NetworkReplicaConfig> replica_config;
	NodePath root_path = NodePath("..");

protected:
	static void _bind_methods();

	void _notification(int p_what);

public:
	PackedStringArray get_configuration_warnings() const override;

	void set_replica_config(Ref<NetworkReplicaConfig> p_config);
	Ref<NetworkReplicaConfig> get_replica_config() const;

	void set_root_path(const NodePath &p_path);
	NodePath get_root_path() const;

	NetworkActor();
};
