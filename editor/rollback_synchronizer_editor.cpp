#include "rollback_synchronizer_editor.h"

#include "../rollback_replica_config.h"
#include "../rollback_synchronizer.h"

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

void RollbackSynchronizerEditor::edit(NetworkActor *p_object) {
	if (current == p_object) {
		return;
	}
	current = p_object;
	if (current) {
		// config = current->get_replication_config();
	} else {
		// config.unref();
	}
	// _update_config();
}

void RollbackSynchronizerEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			// Initialization code can go here.
		} break;
		case NOTIFICATION_EXIT_TREE: {
			// Cleanup code can go here.
		} break;
	}
}

void RollbackSynchronizerEditor::_bind_methods() {
}

RollbackSynchronizerEditor::RollbackSynchronizerEditor() {
	set_v_size_flags(SIZE_EXPAND_FILL);
	set_custom_minimum_size(Size2(0, 200) * EDSCALE);

	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(vb);

	pick_node = memnew(SceneTreeDialog);
	add_child(pick_node);
	pick_node->set_title(TTR("Pick a node to synchronize:"));
	// pick_node->connect("selected", callable_mp(this, &RollbackSynchronizerEditor::_pick_node_selected));
	// pick_node->get_filter_line_edit()->connect(SceneStringName(text_changed), callable_mp(this, &RollbackSynchronizerEditor::_pick_node_filter_text_changed));

	// prop_selector = memnew(PropertySelector);
	// add_child(prop_selector);

	// // Only fixed length types support rollback.
	// Vector<Variant::Type> types = {
	// 	Variant::BOOL,
	// 	Variant::INT,
	// 	Variant::FLOAT,
	// 	Variant::STRING,

	// 	Variant::VECTOR2,
	// 	Variant::VECTOR2I,
	// 	Variant::RECT2,
	// 	Variant::RECT2I,
	// 	Variant::VECTOR3,
	// 	Variant::VECTOR3I,
	// 	Variant::TRANSFORM2D,
	// 	Variant::VECTOR4,
	// 	Variant::VECTOR4I,
	// 	Variant::PLANE,
	// 	Variant::QUATERNION,
	// 	Variant::AABB,
	// 	Variant::BASIS,
	// 	Variant::TRANSFORM3D,
	// 	Variant::PROJECTION,

	// 	Variant::COLOR,
	// };
	// prop_selector->set_type_filter(types);
	// // prop_selector->connect("selected", callable_mp(this, &RollbackSynchronizerEditor::_pick_node_property_selected));

	// HBoxContainer *hb = memnew(HBoxContainer);
	// vb->add_child(hb);

	// add_pick_button = memnew(Button);
	// // add_pick_button->connect(SceneStringName(pressed), callable_mp(this, &RollbackSynchronizerEditor::_pick_new_property));
	// add_pick_button->set_text(TTR("Add property to sync..."));
	// hb->add_child(add_pick_button);

	// VSeparator *vs = memnew(VSeparator);
	// vs->set_custom_minimum_size(Size2(30 * EDSCALE, 0));
	// hb->add_child(vs);
	// hb->add_child(memnew(Label(TTR("Path:"))));

	// np_line_edit = memnew(LineEdit);
	// np_line_edit->set_placeholder(":property");
	// np_line_edit->set_accessibility_name(TTRC("Path:"));
	// np_line_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	// // np_line_edit->connect(SceneStringName(text_submitted), callable_mp(this, &RollbackSynchronizerEditor::_np_text_submitted));
	// hb->add_child(np_line_edit);

	// add_from_path_button = memnew(Button);
	// // add_from_path_button->connect(SceneStringName(pressed), callable_mp(this, &RollbackSynchronizerEditor::_add_pressed));
	// add_from_path_button->set_text(TTR("Add from path"));
	// hb->add_child(add_from_path_button);

	// vs = memnew(VSeparator);
	// vs->set_custom_minimum_size(Size2(30 * EDSCALE, 0));
	// hb->add_child(vs);

	// pin = memnew(Button);
	// pin->set_theme_type_variation(SceneStringName(FlatButton));
	// pin->set_toggle_mode(true);
	// pin->set_tooltip_text(TTR("Pin rollback editor"));
	// hb->add_child(pin);

	// tree = memnew(Tree);
	// tree->set_hide_root(true);
	// tree->set_columns(4);
	// tree->set_column_titles_visible(true);
	// tree->set_column_title(0, TTR("Properties"));
	// tree->set_column_expand(0, true);
	// tree->set_column_title(1, TTR("Spawn"));
	// tree->set_column_expand(1, false);
	// tree->set_column_custom_minimum_width(1, 100);
	// tree->set_column_title(2, TTR("Replicate"));
	// tree->set_column_custom_minimum_width(2, 100);
	// tree->set_column_expand(2, false);
	// tree->set_column_expand(3, false);
	// tree->create_item();
	// // tree->connect("button_clicked", callable_mp(this, &ReplicationEditor::_tree_button_pressed));
	// // tree->connect("item_edited", callable_mp(this, &ReplicationEditor::_tree_item_edited));
	// tree->set_v_size_flags(SIZE_EXPAND_FILL);
	// vb->add_child(tree);

	// // drop_label = memnew(Label);
	// // drop_label->set_focus_mode(FOCUS_ACCESSIBILITY);
	// // drop_label->set_text(TTR("Add properties using the options above, or\ndrag them from the inspector and drop them here."));
	// // drop_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	// // drop_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	// // tree->add_child(drop_label);
	// // drop_label->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	// // SET_DRAG_FORWARDING_CDU(tree, ReplicationEditor);
}
