#pragma once

#include "editor/plugins/editor_plugin.h"
#include "rollback_synchronizer_editor.h"

class RollbackSynchronizerEditor;

class RollbackSynchronizerEditorPlugin : public EditorPlugin {
	GDCLASS(RollbackSynchronizerEditorPlugin, EditorPlugin);

private:
	Button *button = nullptr;
	RollbackSynchronizerEditor *rollback_synchronizer_editor = nullptr;

public:
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	RollbackSynchronizerEditorPlugin();
};
