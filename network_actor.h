#pragma once

#include "network_actor_replica_config.h"

#include "modules/multiplayer/multiplayer_synchronizer.h"
#include "modules/multiplayer/scene_replication_config.h"
#include "scene/main/node.h"

// NetworkEntity is ok
// NetworkObject is misleading...
class NetworkActor : public Node {
	GDCLASS(NetworkActor, Node)

private:
	Ref<SceneReplicationConfig> _replication_config;
	MultiplayerSynchronizer *_multiplayer_synchronizer;

	ObjectID root_node_cache;
	Ref<NetworkActorReplicaConfig> replica_config;
	NodePath root_path = NodePath("..");

	void _start();
	void _stop();
	void reset() {}
	Node *get_root_node();

protected:
	static void _bind_methods();

	void _notification(int p_what);

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	PackedStringArray get_configuration_warnings() const override;

	void set_replica_config(Ref<NetworkActorReplicaConfig> p_config);
	Ref<NetworkActorReplicaConfig> get_replica_config() const;

	void set_root_path(const NodePath &p_path);
	NodePath get_root_path() const;

	virtual void set_multiplayer_authority(int p_peer_id, bool p_recursive = true) override;

	NetworkActor();
	~NetworkActor();
};
