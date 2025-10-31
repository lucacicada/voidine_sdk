#pragma once

#include "rollback_replica_config.h"
#include "scene/main/node.h"

class RollbackSynchronizer : public Node {
	GDCLASS(RollbackSynchronizer, Node)

private:
	Ref<RollbackReplicaConfig> replica_config;
	NodePath root_path = NodePath("..");

protected:
	static void _bind_methods();

	void _notification(int p_what);

public:
	PackedStringArray get_configuration_warnings() const override;

	void set_replica_config(Ref<RollbackReplicaConfig> p_config);
	Ref<RollbackReplicaConfig> get_replica_config() const;

	void set_root_path(const NodePath &p_path);
	NodePath get_root_path() const;

	RollbackSynchronizer();
};
