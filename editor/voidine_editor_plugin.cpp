#include "voidine_editor_plugin.h"

#include "../rollback_synchronizer.h"

#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/settings/editor_command_palette.h"

void VoidineEditorPlugin::edit(Object *p_object) {
	rollback_synchronizer_editor->edit(Object::cast_to<RollbackSynchronizer>(p_object));
}

bool VoidineEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("RollbackSynchronizer");
}

void VoidineEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		button->show();
		EditorNode::get_bottom_panel()->make_item_visible(rollback_synchronizer_editor);
	} else if (!rollback_synchronizer_editor->get_pin()->is_pressed()) {
		if (rollback_synchronizer_editor->is_visible_in_tree()) {
			EditorNode::get_bottom_panel()->hide_bottom_panel();
		}
		button->hide();
	}
}

VoidineEditorPlugin::VoidineEditorPlugin() {
	rollback_synchronizer_editor = memnew(RollbackSynchronizerEditor);

	button = EditorNode::get_bottom_panel()->add_item(TTRC("Rollback"), rollback_synchronizer_editor, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_rollback_bottom_panel", TTRC("Toggle Rollback Bottom Panel")));
	button->hide();
}
