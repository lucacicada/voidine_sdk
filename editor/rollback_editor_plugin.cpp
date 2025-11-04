#include "rollback_editor_plugin.h"

#include "editor/editor_string_names.h"
#include "editor/run/editor_run_bar.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_box.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/line_edit.h"
#include "scene/main/timer.h"

void RollbackEditorProfiler::started() {
	_clear_pressed();
	activate->set_disabled(false);

	if (EditorSettings::get_singleton()->get_project_metadata("debug_options", "autostart_rollback_profiler", false)) {
		set_profiling(true);
		refresh_timer->start();
	}
}

void RollbackEditorProfiler::stopped() {
	activate->set_disabled(true);
	set_profiling(false);
	refresh_timer->stop();
}

void RollbackEditorProfiler::set_profiling(bool p_pressed) {
	activate->set_pressed(p_pressed);
	_update_activate_button_text();
	emit_signal(SNAME("enable_profiling"), activate->is_pressed());
}

void RollbackEditorProfiler::_bind_methods() {
	ADD_SIGNAL(MethodInfo("enable_profiling", PropertyInfo(Variant::BOOL, "enable")));
	ADD_SIGNAL(MethodInfo("open_request", PropertyInfo(Variant::STRING, "path")));
}

void RollbackEditorProfiler::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			_update_activate_button_text();
			clear_button->set_button_icon(get_theme_icon(SNAME("Clear"), EditorStringName(EditorIcons)));
		} break;
	}
}

void RollbackEditorProfiler::_update_activate_button_text() {
	if (activate->is_pressed()) {
		activate->set_button_icon(get_theme_icon(SNAME("Stop"), EditorStringName(EditorIcons)));
		activate->set_text(TTR("Stop"));
	} else {
		activate->set_button_icon(get_theme_icon(SNAME("Play"), EditorStringName(EditorIcons)));
		activate->set_text(TTR("Start"));
	}
}

void RollbackEditorProfiler::_activate_pressed() {
	_update_activate_button_text();

	if (activate->is_pressed()) {
		// refresh_timer->start();
	} else {
		// refresh_timer->stop();
	}

	emit_signal(SNAME("enable_profiling"), activate->is_pressed());
}

void RollbackEditorProfiler::_clear_pressed() {
	clear_button->set_disabled(true);
}

void RollbackEditorProfiler::_autostart_toggled(bool p_toggled_on) {
	EditorSettings::get_singleton()->set_project_metadata("debug_options", "autostart_rollback_profiler", p_toggled_on);
	EditorRunBar::get_singleton()->update_profiler_autostart_indicator();
}

RollbackEditorProfiler::RollbackEditorProfiler() {
	FlowContainer *container = memnew(FlowContainer);
	container->add_theme_constant_override(SNAME("h_separation"), 8 * EDSCALE);
	container->add_theme_constant_override(SNAME("v_separation"), 2 * EDSCALE);
	add_child(container);

	activate = memnew(Button);
	activate->set_toggle_mode(true);
	activate->set_text(TTR("Start"));
	activate->set_disabled(true);
	activate->connect(SceneStringName(pressed), callable_mp(this, &RollbackEditorProfiler::_activate_pressed));
	container->add_child(activate);

	clear_button = memnew(Button);
	clear_button->set_text(TTR("Clear"));
	clear_button->set_disabled(true);
	clear_button->connect(SceneStringName(pressed), callable_mp(this, &RollbackEditorProfiler::_clear_pressed));
	container->add_child(clear_button);

	CheckBox *autostart_checkbox = memnew(CheckBox);
	autostart_checkbox->set_text(TTR("Autostart"));
	autostart_checkbox->set_pressed(EditorSettings::get_singleton()->get_project_metadata("debug_options", "autostart_rollback_profiler", false));
	autostart_checkbox->connect(SceneStringName(toggled), callable_mp(this, &RollbackEditorProfiler::_autostart_toggled));
	container->add_child(autostart_checkbox);

	Control *c = memnew(Control);
	c->set_h_size_flags(SIZE_EXPAND_FILL);
	container->add_child(c);

	HBoxContainer *hb = memnew(HBoxContainer);
	hb->add_theme_constant_override(SNAME("separation"), 8 * EDSCALE);
	container->add_child(hb);

	Label *lb = memnew(Label);
	lb->set_focus_mode(FOCUS_ACCESSIBILITY);
	lb->set_text(TTR("Down", "Network"));
	hb->add_child(lb);

	refresh_timer = memnew(Timer);
	refresh_timer->set_wait_time(0.5);
	// refresh_timer->connect("timeout", callable_mp(this, &EditorNetworkProfiler::_refresh));
	add_child(refresh_timer);
}

// RollbackEditorDebuggerPlugin

bool RollbackEditorDebuggerPlugin::has_capture(const String &p_capture) const {
	return p_capture == "rollback";
}

bool RollbackEditorDebuggerPlugin::capture(const String &p_message, const Array &p_data, int p_session) {
	ERR_FAIL_COND_V(!profilers.has(p_session), false);
	RollbackEditorProfiler *profiler = profilers[p_session];

	if (p_message == "rollback:input") {
		return true;
	}

	return false;
}

void RollbackEditorDebuggerPlugin::_profiler_activate(bool p_enable, int p_session_id) {
	Ref<EditorDebuggerSession> session = get_session(p_session_id);
	ERR_FAIL_COND(session.is_null());
	session->toggle_profiler("rollback:input", p_enable);
}

void RollbackEditorDebuggerPlugin::setup_session(int p_session_id) {
	Ref<EditorDebuggerSession> session = get_session(p_session_id);
	ERR_FAIL_COND(session.is_null());
	RollbackEditorProfiler *profiler = memnew(RollbackEditorProfiler);
	profiler->connect("enable_profiling", callable_mp(this, &RollbackEditorDebuggerPlugin::_profiler_activate).bind(p_session_id));
	profiler->set_name(TTR("Rollback Profiler"));
	session->connect("started", callable_mp(profiler, &RollbackEditorProfiler::started));
	session->connect("stopped", callable_mp(profiler, &RollbackEditorProfiler::stopped));
	session->add_session_tab(profiler);
	profilers[p_session_id] = profiler;
}

// RollbackEditorPlugin

RollbackEditorPlugin::RollbackEditorPlugin() {
	debugger.instantiate();
}

void RollbackEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			add_debugger_plugin(debugger);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			remove_debugger_plugin(debugger);
		}
	}
}
