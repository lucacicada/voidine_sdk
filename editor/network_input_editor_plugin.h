#pragma once

#include "editor/plugins/editor_plugin.h"
#include "network_input_editor.h"

class NetworkInputEditor;

class NetworkInputEditorPlugin : public EditorPlugin {
	GDCLASS(NetworkInputEditorPlugin, EditorPlugin);

private:
	Button *button = nullptr;
	NetworkInputEditor *network_input_editor = nullptr;

public:
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	NetworkInputEditorPlugin();
};
