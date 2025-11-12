#include "network_input_editor.h"

#include "../network_input.h"
#include "../network_input_replica_config.h"

#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/inspector/property_selector.h"
#include "editor/scene/scene_tree_editor.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/separator.h"
#include "scene/gui/tree.h"

void NetworkInputEditor::edit(NetworkInput *p_object) {
	if (current == p_object) {
		return;
	}
	current = p_object;
	if (current) {
		config = current->get_replica_config();
	} else {
		config.unref();
	}
	_update_config();
}

void NetworkInputEditor::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorThemeManager::is_generated_theme_outdated()) {
				break;
			}
			[[fallthrough]];
		}
		case NOTIFICATION_ENTER_TREE: {
			add_theme_style_override(SceneStringName(panel), EditorNode::get_singleton()->get_editor_theme()->get_stylebox(SceneStringName(panel), SNAME("Panel")));
			add_pick_button->set_button_icon(get_theme_icon(SNAME("Add"), EditorStringName(EditorIcons)));
			pin->set_button_icon(get_theme_icon(SNAME("Pin"), EditorStringName(EditorIcons)));
		} break;
	}
}

void NetworkInputEditor::_pick_new_property() {
	if (current == nullptr) {
		EditorNode::get_singleton()->show_warning(TTR("Select a replicator node in order to pick a property to add to it."));
		return;
	}
	prop_selector->select_property_from_instance(current);
}

void NetworkInputEditor::_pick_property_selected(String p_name) {
	String adding_prop_path = p_name;

	// _add_sync_property(adding_prop_path);
	String p_path = adding_prop_path;

	ERR_FAIL_NULL(current);

	config = current->get_replica_config();

	if (config.is_valid() && config->has_property(p_path)) {
		EditorNode::get_singleton()->show_warning(TTR("Input already exists."));
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add input to synchronizer"));

	if (config.is_null()) {
		config.instantiate();
		current->set_replica_config(config);
		undo_redo->add_do_method(current, "set_replica_config", config);
		undo_redo->add_undo_method(current, "set_replica_config", Ref<NetworkInputReplicaConfig>());
		_update_config();
	}

	undo_redo->add_do_method(config.ptr(), "add_property", p_path);
	undo_redo->add_undo_method(config.ptr(), "remove_property", p_path);
	undo_redo->add_do_method(this, "_update_config");
	undo_redo->add_undo_method(this, "_update_config");
	undo_redo->commit_action();
}

void NetworkInputEditor::_tree_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}

	TreeItem *ti = Object::cast_to<TreeItem>(p_item);
	if (!ti) {
		return;
	}
	deleting = ti->get_metadata(0);
	delete_dialog->set_text(TTR("Delete Property?") + "\n\"" + ti->get_text(0) + "\"");
	delete_dialog->popup_centered();
}

struct PropertySizeSort {
	_ALWAYS_INLINE_ bool operator()(const Pair<NodePath, Pair<int, int>> &a, const Pair<NodePath, Pair<int, int>> &b) const {
		if (a.second.first != b.second.first) {
			return a.second.first > b.second.first;
		}
		return a.second.second < b.second.second;
	}
};

void NetworkInputEditor::_optimize_pressed() {
	if (config.is_null()) {
		return;
	}

	TypedArray<NodePath> props = config->get_properties();
	LocalVector<Pair<NodePath, Pair<int, int>>> prop_sizes;
	for (int i = 0; i < props.size(); i++) {
		const NodePath path = props[i];
		const Variant &prop_value = current ? current->get_indexed(path.get_names()) : Variant();
		int s = get_type_size_in_bits(prop_value.get_type());
		prop_sizes.push_back(Pair<NodePath, Pair<int, int>>(path, Pair<int, int>(s, i)));
	}

	prop_sizes.sort_custom<PropertySizeSort>();

	config->reset_state();
	for (uint32_t i = 0; i < prop_sizes.size(); i++) {
		config->add_property(prop_sizes[i].first);
	}
	_update_config();
}

void NetworkInputEditor::_update_config() {
	deleting = NodePath();
	tree->clear();
	tree->create_item();
	drop_label->set_visible(true);
	if (config.is_null()) {
		return;
	}
	TypedArray<NodePath> props = config->get_properties();
	if (props.size()) {
		drop_label->set_visible(false);
	}

	int size_in_bits = 0;
	int unused_bits = 0;

	Vector<Variant> state_vars;
	Vector<const Variant *> varp;

	state_vars.resize(props.size());
	varp.resize(state_vars.size());

	for (int i = 0; i < props.size(); i++) {
		const NodePath path = props[i];
		_add_property(path);

		//
		const Variant &prop_value = current ? current->get_indexed(path.get_names()) : Variant();
		int s = get_type_size_in_bits(prop_value.get_type());

		state_vars.write[i] = prop_value;
		varp.write[i] = &state_vars[i];

		if (s <= 0) {
			continue;
		}

		// 8-bit aligned
		if (s != 1) { // bool is 1 bit
			if (size_in_bits % 8 != 0) {
				int padding_size = 8 - (size_in_bits % 8);
				size_in_bits += padding_size;
				unused_bits += padding_size;
			}
		}

		size_in_bits += s;
	}

	if (size_in_bits % 8 != 0) {
		int padding = 8 - (size_in_bits % 8);
		size_in_bits += padding;
		unused_bits += padding;
	}

	int size;
	MultiplayerAPI::encode_and_compress_variants(varp.ptrw(), varp.size(), nullptr, size);

	stats_label->set_text(vformat(TTR("Struct size: %d - Padding Bits: %d bits - %d"), size_in_bits / 8, unused_bits, size));
}

void NetworkInputEditor::_add_property(const NodePath &p_property) {
	String prop = String(p_property);
	TreeItem *item = tree->create_item();
	item->set_selectable(0, true);
	item->set_selectable(1, false);
	item->set_selectable(2, false);
	item->set_selectable(3, false);
	item->set_selectable(4, false);
	item->set_text(0, prop);
	item->set_metadata(0, prop);

	item->set_text_alignment(3, HORIZONTAL_ALIGNMENT_CENTER);
	item->set_cell_mode(3, TreeItem::CELL_MODE_CHECK);
	item->set_checked(3, false);
	item->set_editable(3, true);

	item->add_button(4, get_theme_icon(SNAME("Remove"), EditorStringName(EditorIcons)));

	bool prop_valid = false;
	const Variant &prop_value = current ? current->get_indexed(p_property.get_names(), &prop_valid) : Variant();

	item->set_text(1, Variant::get_type_name(prop_value.get_type()));
	item->set_icon(1, get_theme_icon(Variant::get_type_name(prop_value.get_type()), EditorStringName(EditorIcons)));

	if (!prop_valid) {
		item->set_icon(1, get_theme_icon(SNAME("ImportFail"), EditorStringName(EditorIcons)));
	}

	item->set_text(2, vformat("%d-bit", get_type_size_in_bits(prop_value.get_type())));
}

void NetworkInputEditor::_dialog_closed(bool p_confirmed) {
	if (deleting.is_empty() || config.is_null()) {
		return;
	}
	if (p_confirmed) {
		const NodePath prop = deleting;
		int idx = config->property_get_index(prop);
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Remove Property"));
		undo_redo->add_do_method(config.ptr(), "remove_property", prop);
		undo_redo->add_undo_method(config.ptr(), "add_property", prop, idx);
		undo_redo->add_do_method(this, "_update_config");
		undo_redo->add_undo_method(this, "_update_config");
		undo_redo->commit_action();
	}
	deleting = NodePath();
}

void NetworkInputEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_config"), &NetworkInputEditor::_update_config);
}

Variant NetworkInputEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	TreeItem *selected = tree->get_selected();
	if (!selected) {
		return Variant();
	}

	String name = selected->get_text(0);
	Label *label = memnew(Label(name));
	label->set_theme_type_variation("HeaderSmall");
	label->set_modulate(Color(1, 1, 1, 1.0f));
	label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	tree->set_drag_preview(label);

	get_viewport()->gui_set_drag_description(vformat(RTR("Action %s"), name));

	Dictionary drag_data;
	drag_data["replica_property_name"] = name;
	drag_data["source"] = selected->get_instance_id();

	tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN);

	return drag_data;
}

bool NetworkInputEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	Dictionary d = p_data;
	if (!d.has("replica_property_name")) {
		return false;
	}

	TreeItem *source = Object::cast_to<TreeItem>(ObjectDB::get_instance(d["source"].operator ObjectID()));
	TreeItem *selected = tree->get_selected();

	TreeItem *item = (p_point == Vector2(Math::INF, Math::INF)) ? selected : tree->get_item_at_position(p_point);
	if (!selected || !item || item == source) {
		return false;
	}

	return true;
}

void NetworkInputEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	TreeItem *selected = tree->get_selected();
	TreeItem *target = (p_point == Vector2(Math::INF, Math::INF)) ? selected : tree->get_item_at_position(p_point);
	if (!target) {
		return;
	}

	bool drop_above = ((p_point == Vector2(Math::INF, Math::INF)) ? tree->get_drop_section_at_position(tree->get_item_rect(target).position) : tree->get_drop_section_at_position(p_point)) == -1;

	if (config.is_valid()) {
		int idx = config->property_get_index(NodePath(target->get_metadata(0)));
		config->add_property(selected->get_text(0), drop_above ? idx : idx + 1);
		_update_config();
	}
}

NetworkInputEditor::NetworkInputEditor() {
	set_v_size_flags(SIZE_EXPAND_FILL);
	set_custom_minimum_size(Size2(0, 200) * EDSCALE);

	delete_dialog = memnew(ConfirmationDialog);
	delete_dialog->connect("canceled", callable_mp(this, &NetworkInputEditor::_dialog_closed).bind(false));
	delete_dialog->connect(SceneStringName(confirmed), callable_mp(this, &NetworkInputEditor::_dialog_closed).bind(true));
	add_child(delete_dialog);

	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(vb);

	prop_selector = memnew(PropertySelector);
	add_child(prop_selector);

	// Only fixed length types support rollback.
	Vector<Variant::Type> types = {
		Variant::BOOL,
		Variant::INT,
		Variant::FLOAT,
		Variant::VECTOR2,
		Variant::VECTOR2I,
		Variant::VECTOR3,
		Variant::VECTOR3I,
		Variant::VECTOR4,
		Variant::VECTOR4I,
		Variant::RECT2,
		Variant::RECT2I,
	};
	prop_selector->set_type_filter(types);
	prop_selector->connect("selected", callable_mp(this, &NetworkInputEditor::_pick_property_selected));

	HBoxContainer *hb = memnew(HBoxContainer);
	vb->add_child(hb);

	add_pick_button = memnew(Button);
	add_pick_button->connect(SceneStringName(pressed), callable_mp(this, &NetworkInputEditor::_pick_new_property));
	add_pick_button->set_text(TTR("Add input property..."));
	hb->add_child(add_pick_button);

	VSeparator *vs = memnew(VSeparator);
	vs->set_custom_minimum_size(Size2(30 * EDSCALE, 0));
	hb->add_child(vs);

	stats_label = memnew(Label);
	stats_label->set_h_size_flags(SIZE_EXPAND_FILL);
	stats_label->set_text("Stats: N/A");
	hb->add_child(stats_label);

	optimize_button = memnew(Button);
	optimize_button->connect(SceneStringName(pressed), callable_mp(this, &NetworkInputEditor::_optimize_pressed));
	optimize_button->set_text(TTR("Optimize"));
	hb->add_child(optimize_button);

	vs = memnew(VSeparator);
	vs->set_custom_minimum_size(Size2(30 * EDSCALE, 0));
	hb->add_child(vs);

	pin = memnew(Button);
	pin->set_theme_type_variation(SceneStringName(FlatButton));
	pin->set_toggle_mode(true);
	pin->set_tooltip_text(TTR("Pin input editor"));
	hb->add_child(pin);

	tree = memnew(Tree);
	tree->set_hide_root(true);
	tree->set_columns(5);
	tree->set_column_titles_visible(true);
	tree->set_column_title(0, TTR("Properties"));
	tree->set_column_expand(0, true);
	tree->set_column_title(1, TTR("Type"));
	tree->set_column_expand(1, false);
	tree->set_column_custom_minimum_width(1, 100);
	tree->set_column_title(2, TTR("Encoding"));
	tree->set_column_custom_minimum_width(2, 100);
	tree->set_column_expand(2, false);
	tree->set_column_title(3, TTR("Sample"));
	tree->set_column_custom_minimum_width(3, 100);
	tree->set_column_expand(3, false);
	tree->set_column_expand(4, false);
	tree->create_item();
	tree->connect("button_clicked", callable_mp(this, &NetworkInputEditor::_tree_button_pressed));
	// tree->connect("item_edited", callable_mp(this, &NetworkInputEditor::_tree_item_edited));
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	vb->add_child(tree);

	drop_label = memnew(Label);
	drop_label->set_focus_mode(FOCUS_ACCESSIBILITY);
	drop_label->set_text(TTR("Add properties using the options above, or\ndrag them from the inspector and drop them here."));
	drop_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	drop_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	tree->add_child(drop_label);
	drop_label->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	SET_DRAG_FORWARDING_GCD(tree, NetworkInputEditor);
}
