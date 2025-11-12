#include "network_actor.h"

#include "core/config/engine.h"
#include "scene/2d/node_2d.h"
#include "scene/main/multiplayer_api.h"

bool NetworkActor::_set(const StringName &p_name, const Variant &p_value) {
	print_line(vformat("SET: %s", p_name));

	if (p_name == "state_position") {
		return true;
	}

	return false;
}

bool NetworkActor::_get(const StringName &p_name, Variant &r_ret) const {
	print_line(vformat("GET: %s", p_name));

	String prop_name = p_name;
	if (p_name == "state_position") {
		r_ret = Vector2();
		return true;
	}

	return false;
}

void NetworkActor::_get_property_list(List<PropertyInfo> *p_list) const {
	print_line(vformat("_get_property_list"));
	p_list->push_back(PropertyInfo(Variant::VECTOR2, "state_position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
}

void NetworkActor::_start() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	root_node_cache = ObjectID();
	reset();
	Node *node = is_inside_tree() ? get_node_or_null(root_path) : nullptr;
	if (node) {
		Node2D *node_2d = Object::cast_to<Node2D>(node);
		if (node_2d) {
		}

		root_node_cache = node->get_instance_id();
		get_multiplayer()->object_configuration_add(node, this);
	}
}

void NetworkActor::_stop() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	root_node_cache = ObjectID();
	Node *node = is_inside_tree() ? get_node_or_null(root_path) : nullptr;
	if (node) {
		get_multiplayer()->object_configuration_remove(node, this);
	}
	reset();
}

void NetworkActor::_notification(int p_what) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	if (root_path.is_empty()) {
		return;
	}

	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_start();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_stop();
		} break;
	}
}

Node *NetworkActor::get_root_node() {
	return root_node_cache.is_valid() ? ObjectDB::get_instance<Node>(root_node_cache) : nullptr;
}

void NetworkActor::set_replica_config(Ref<NetworkActorReplicaConfig> p_config) {
	replica_config = p_config;
}

Ref<NetworkActorReplicaConfig> NetworkActor::get_replica_config() const {
	return replica_config;
}

void NetworkActor::set_root_path(const NodePath &p_path) {
	if (p_path == root_path) {
		return;
	}
	_stop();
	root_path = p_path;
	_start();
	update_configuration_warnings();
}

NodePath NetworkActor::get_root_path() const {
	return root_path;
}

void NetworkActor::set_multiplayer_authority(int p_peer_id, bool p_recursive) {
	if (get_multiplayer_authority() == p_peer_id) {
		return;
	}
	_stop();
	Node::set_multiplayer_authority(p_peer_id, p_recursive);
	_start();
}

PackedStringArray NetworkActor::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (root_path.is_empty() || !has_node(root_path)) {
		warnings.push_back(RTR("A valid NodePath must be set in the \"Root Path\" property in order for NetworkActor to be able to synchronize properties."));
	}

	return warnings;
}

void NetworkActor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_root_path", "path"), &NetworkActor::set_root_path);
	ClassDB::bind_method(D_METHOD("get_root_path"), &NetworkActor::get_root_path);

	ClassDB::bind_method(D_METHOD("set_replica_config", "config"), &NetworkActor::set_replica_config);
	ClassDB::bind_method(D_METHOD("get_replica_config"), &NetworkActor::get_replica_config);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root_path"), "set_root_path", "get_root_path");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "replica_config", PROPERTY_HINT_RESOURCE_TYPE, "NetworkActorReplicaConfig", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_replica_config", "get_replica_config");
}

NetworkActor::NetworkActor() {
	_replication_config.instantiate();
	_multiplayer_synchronizer = memnew(MultiplayerSynchronizer);
	_multiplayer_synchronizer->set_name("Replica");
	_multiplayer_synchronizer->set_replication_interval(1.0 / 60.0);
	_multiplayer_synchronizer->set_delta_interval(1.0 / 5.0);
	_multiplayer_synchronizer->set_replication_config(_replication_config);

	add_child(_multiplayer_synchronizer, true, INTERNAL_MODE_FRONT);
}

NetworkActor::~NetworkActor() {
	_replication_config.unref();
}
