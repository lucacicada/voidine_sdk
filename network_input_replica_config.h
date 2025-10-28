#pragma once

#include "core/io/resource.h"
#include "core/variant/typed_array.h"

class NetworkInputReplicaConfig : public Resource {
	GDCLASS(NetworkInputReplicaConfig, Resource);
	OBJ_SAVE_TYPE(NetworkInputReplicaConfig);

private:
	struct InputProperty {
		NodePath name;

		bool operator==(const InputProperty &p_to) {
			return name == p_to.name;
		}

		InputProperty() {}

		InputProperty(const NodePath &p_name) {
			name = p_name;
		}
	};

	List<InputProperty> properties;

protected:
	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	TypedArray<NodePath> get_properties() const;
	void add_property(const NodePath &p_path, int p_index = -1);
	void remove_property(const NodePath &p_path);
	bool has_property(const NodePath &p_path) const;
	int property_get_index(const NodePath &p_path) const;
	// void property_set_index(const NodePath &p_path, int p_index);
	void reset_state(); // clear_properties

	NetworkInputReplicaConfig() {}
};
