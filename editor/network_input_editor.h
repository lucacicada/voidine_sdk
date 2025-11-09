#pragma once

#include "../network_input.h"
#include "../network_input_replica_config.h"

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"

class ConfirmationDialog;
class AcceptDialog;
class LineEdit;
class Tree;
class TreeItem;
class PropertySelector;
class SceneTreeDialog;
class NetworkActor;

class NetworkInputEditor : public VBoxContainer {
	GDCLASS(NetworkInputEditor, VBoxContainer);

private:
	static const uint32_t TYPE_BOOL_SIZE = 1;
	static const uint32_t TYPE_INT_SIZE = 64;
	static const uint32_t TYPE_FLOAT_SIZE = 64;

	static const uint32_t TYPE_VECTOR2_SIZE = (sizeof(real_t) * 8) * 2; // x, y
	static const uint32_t TYPE_VECTOR2I_SIZE = (sizeof(real_t) * 8) * 2; // x, y
	static const uint32_t TYPE_VECTOR3_SIZE = (sizeof(real_t) * 8) * 3; // x, y, z
	static const uint32_t TYPE_VECTOR3I_SIZE = (sizeof(real_t) * 8) * 3; // x, y, z
	static const uint32_t TYPE_VECTOR4_SIZE = (sizeof(real_t) * 8) * 4; // x, y, z, w
	static const uint32_t TYPE_VECTOR4I_SIZE = (sizeof(real_t) * 8) * 4; // x, y, z, w
	static const uint32_t TYPE_RECT2_SIZE = (sizeof(real_t) * 8) * 4; // position.x, position.y, size.x, size.y
	static const uint32_t TYPE_RECT2I_SIZE = (sizeof(real_t) * 8) * 4; // position.x, position.y, size.x, size.y

	static int get_type_size_in_bits(Variant::Type p_type) {
		switch (p_type) {
			case Variant::BOOL:
				return TYPE_BOOL_SIZE;
			case Variant::INT:
				return TYPE_INT_SIZE;
			case Variant::FLOAT:
				return TYPE_FLOAT_SIZE;
			case Variant::VECTOR2:
				return TYPE_VECTOR2_SIZE;
			case Variant::VECTOR2I:
				return TYPE_VECTOR2I_SIZE;
			case Variant::VECTOR3:
				return TYPE_VECTOR3_SIZE;
			case Variant::VECTOR3I:
				return TYPE_VECTOR3I_SIZE;
			case Variant::VECTOR4:
				return TYPE_VECTOR4_SIZE;
			case Variant::VECTOR4I:
				return TYPE_VECTOR4I_SIZE;
			case Variant::RECT2:
				return TYPE_RECT2_SIZE;
			case Variant::RECT2I:
				return TYPE_RECT2I_SIZE;
			default:
				return 0;
		}
	}

	NetworkInput *current = nullptr;
	Ref<NetworkInputReplicaConfig> config;
	NodePath deleting;

	ConfirmationDialog *delete_dialog = nullptr;
	Button *pin = nullptr;
	Button *add_pick_button = nullptr;
	Button *optimize_button = nullptr;
	PropertySelector *prop_selector = nullptr;
	Label *drop_label = nullptr;
	Label *stats_label = nullptr;
	Tree *tree = nullptr;

	void _pick_new_property();
	void _pick_property_selected(String p_name);
	void _update_config();
	void _dialog_closed(bool p_confirmed);
	void _add_property(const NodePath &p_property);
	void _tree_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button);
	void _optimize_pressed();

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

protected:
	static void _bind_methods();

	void _notification(int p_what);

public:
	void edit(NetworkInput *p_object);
	NetworkInput *get_current() const { return current; }

	Button *get_pin() { return pin; }
	NetworkInputEditor();
};
