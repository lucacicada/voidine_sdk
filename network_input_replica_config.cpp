#include "network_input_replica_config.h"

bool NetworkInputReplicaConfig::_set(const StringName &p_name, const Variant &p_value) {
	String prop_name = p_name;

	if (prop_name.begins_with("properties/")) {
		int idx = prop_name.get_slicec('/', 1).to_int();
		String what = prop_name.get_slicec('/', 2);

		if (properties.size() == idx && what == "path") {
			ERR_FAIL_COND_V(p_value.get_type() != Variant::NODE_PATH, false);
			NodePath path = p_value;
			ERR_FAIL_COND_V(path.is_empty() || path.get_name_count() == 0, false);
			add_property(path);
			return true;
		}
	}
	return false;
}

bool NetworkInputReplicaConfig::_get(const StringName &p_name, Variant &r_ret) const {
	String prop_name = p_name;

	if (prop_name.begins_with("properties/")) {
		int idx = prop_name.get_slicec('/', 1).to_int();
		String what = prop_name.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(idx, properties.size(), false);
		const InputProperty &prop = properties.get(idx);
		if (what == "path") {
			r_ret = prop.name;
			return true;
		}
	}
	return false;
}

void NetworkInputReplicaConfig::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < properties.size(); i++) {
		p_list->push_back(PropertyInfo(Variant::STRING, "properties/" + itos(i) + "/path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
	}
}

void NetworkInputReplicaConfig::reset_state() {
	properties.clear();
}

TypedArray<NodePath> NetworkInputReplicaConfig::get_properties() const {
	TypedArray<NodePath> paths;
	for (const InputProperty &prop : properties) {
		paths.push_back(prop.name);
	}
	return paths;
}

void NetworkInputReplicaConfig::add_property(const NodePath &p_path, int p_index) {
	ERR_FAIL_COND(p_path == NodePath());

	properties.erase(p_path);

	if (p_index < 0 || p_index >= properties.size()) {
		properties.push_back(InputProperty(p_path));
		return;
	}

	List<InputProperty>::Element *I = properties.front();
	int c = 0;
	while (c < p_index) {
		I = I->next();
		c++;
	}
	properties.insert_before(I, InputProperty(p_path));
}

void NetworkInputReplicaConfig::remove_property(const NodePath &p_path) {
	properties.erase(p_path);
}

bool NetworkInputReplicaConfig::has_property(const NodePath &p_path) const {
	for (const InputProperty &property : properties) {
		if (property.name == p_path) {
			return true;
		}
	}
	return false;
}

int NetworkInputReplicaConfig::property_get_index(const NodePath &p_path) const {
	int i = 0;
	for (List<InputProperty>::ConstIterator itr = properties.begin(); itr != properties.end(); ++itr, ++i) {
		if (itr->name == p_path) {
			return i;
		}
	}
	ERR_FAIL_V(-1);
}

// void NetworkInputReplicaConfig::property_set_index(const NodePath &p_path, int p_index) {
// 	List<InputProperty>::Element *E = properties.find(p_path);
// 	ERR_FAIL_COND(!E);

// 	InputProperty prop = E->get();
// 	properties.erase(E);

// 	if (p_index < 0 || p_index >= properties.size()) {
// 		properties.push_back(prop);
// 		return;
// 	}

// 	List<InputProperty>::Element *I = properties.front();
// 	int c = 0;
// 	while (c < p_index) {
// 		I = I->next();
// 		c++;
// 	}
// 	properties.insert_before(I, prop);
// }

void NetworkInputReplicaConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_properties"), &NetworkInputReplicaConfig::get_properties);
	ClassDB::bind_method(D_METHOD("add_property", "path", "index"), &NetworkInputReplicaConfig::add_property, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("has_property", "path"), &NetworkInputReplicaConfig::has_property);
	ClassDB::bind_method(D_METHOD("remove_property", "path"), &NetworkInputReplicaConfig::remove_property);
	ClassDB::bind_method(D_METHOD("property_get_index", "path"), &NetworkInputReplicaConfig::property_get_index);
	// ClassDB::bind_method(D_METHOD("property_set_index", "path", "index"), &NetworkInputReplicaConfig::property_set_index);
}
