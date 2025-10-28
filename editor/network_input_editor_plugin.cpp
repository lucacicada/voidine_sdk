#include "network_input_editor_plugin.h"

#include "../network_input.h"

#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/settings/editor_command_palette.h"

void NetworkInputEditorPlugin::edit(Object *p_object) {
	network_input_editor->edit(Object::cast_to<NetworkInput>(p_object));
}

bool NetworkInputEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("NetworkInput");
}

void NetworkInputEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		button->show();
		EditorNode::get_bottom_panel()->make_item_visible(network_input_editor);
	} else {
		if (network_input_editor->is_visible_in_tree()) {
			EditorNode::get_bottom_panel()->hide_bottom_panel();
		}
		button->hide();
	}
}

NetworkInputEditorPlugin::NetworkInputEditorPlugin() {
	network_input_editor = memnew(NetworkInputEditor);

	button = EditorNode::get_bottom_panel()->add_item(TTRC("Input"), network_input_editor, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_input_bottom_panel", TTRC("Toggle Input Bottom Panel")));
	button->hide();
}
