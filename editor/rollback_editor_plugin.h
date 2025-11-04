#pragma once

#include "editor/debugger/editor_debugger_plugin.h"
#include "editor/plugins/editor_plugin.h"

#include "scene/debugger/scene_debugger.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tree.h"

class RollbackEditorProfiler : public VBoxContainer {
	GDCLASS(RollbackEditorProfiler, VBoxContainer);

private:
	Timer *refresh_timer = nullptr;
	Button *activate = nullptr;
	Button *clear_button = nullptr;

	void _update_activate_button_text();
	void _activate_pressed();
	void _clear_pressed();
	void _autostart_toggled(bool p_toggled_on);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_profiling(bool p_pressed);
	void started();
	void stopped();

	RollbackEditorProfiler();
};

class RollbackEditorDebuggerPlugin : public EditorDebuggerPlugin {
	GDCLASS(RollbackEditorDebuggerPlugin, EditorDebuggerPlugin);

private:
	HashMap<int, RollbackEditorProfiler *> profilers;

	void _profiler_activate(bool p_enable, int p_session_id);

public:
	virtual bool has_capture(const String &p_capture) const override;
	virtual bool capture(const String &p_message, const Array &p_data, int p_index) override;
	virtual void setup_session(int p_session_id) override;
};

class RollbackEditorPlugin : public EditorPlugin {
	GDCLASS(RollbackEditorPlugin, EditorPlugin);

private:
	Ref<RollbackEditorDebuggerPlugin> debugger;

protected:
	void _notification(int p_what);

public:
	RollbackEditorPlugin();
};
