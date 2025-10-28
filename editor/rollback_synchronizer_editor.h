#pragma once

#include "../rollback_replica_config.h"
#include "../rollback_synchronizer.h"

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"

class ConfirmationDialog;
class AcceptDialog;
class LineEdit;
class Tree;
class TreeItem;
class PropertySelector;
class SceneTreeDialog;
class RollbackSynchronizer;

class RollbackSynchronizerEditor : public VBoxContainer {
	GDCLASS(RollbackSynchronizerEditor, VBoxContainer);

private:
	RollbackSynchronizer *current = nullptr;

	Button *pin = nullptr;
	// Button *add_pick_button = nullptr;
	// PropertySelector *prop_selector = nullptr;
	SceneTreeDialog *pick_node = nullptr;
	// Button *add_from_path_button = nullptr;
	// LineEdit *np_line_edit = nullptr;
	// Label *drop_label = nullptr;
	// Tree *tree = nullptr;

protected:
	static void _bind_methods();

	void _notification(int p_what);

public:
	void edit(RollbackSynchronizer *p_object);
	RollbackSynchronizer *get_current() const { return current; }

	Button *get_pin() { return pin; }
	RollbackSynchronizerEditor();
};
