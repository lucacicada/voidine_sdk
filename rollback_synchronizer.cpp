#include "rollback_synchronizer.h"
#include "rollback_tree.h"

void RollbackSynchronizer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			RollbackTree *rollback_tree = Object::cast_to<RollbackTree>(get_tree());
			if (rollback_tree) {
				// rollback_tree->add_synchronizer(this);
			}
			break;
		}
		case NOTIFICATION_EXIT_TREE: {
			RollbackTree *rollback_tree = Object::cast_to<RollbackTree>(get_tree());
			if (rollback_tree) {
				// rollback_tree->remove_synchronizer(this);
			}
			break;
		}
	}
}

void RollbackSynchronizer::set_replica_config(Ref<RollbackReplicaConfig> p_config) {
	replica_config = p_config;
}

Ref<RollbackReplicaConfig> RollbackSynchronizer::get_replica_config() const {
	return replica_config;
}

void RollbackSynchronizer::set_root_path(const NodePath &p_path) {
	if (p_path == root_path) {
		return;
	}
	// _stop();
	root_path = p_path;
	// _start();
	update_configuration_warnings();
}

NodePath RollbackSynchronizer::get_root_path() const {
	return root_path;
}

PackedStringArray RollbackSynchronizer::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (root_path.is_empty() || !has_node(root_path)) {
		warnings.push_back(RTR("A valid NodePath must be set in the \"Root Path\" property in order for RollbackSynchronizer to be able to synchronize properties."));
	}

	return warnings;
}

void RollbackSynchronizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_root_path", "path"), &RollbackSynchronizer::set_root_path);
	ClassDB::bind_method(D_METHOD("get_root_path"), &RollbackSynchronizer::get_root_path);

	ClassDB::bind_method(D_METHOD("set_replica_config", "config"), &RollbackSynchronizer::set_replica_config);
	ClassDB::bind_method(D_METHOD("get_replica_config"), &RollbackSynchronizer::get_replica_config);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "replica_config", PROPERTY_HINT_RESOURCE_TYPE, "RollbackReplicaConfig", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_replica_config", "get_replica_config");
}

RollbackSynchronizer::RollbackSynchronizer() {
}
