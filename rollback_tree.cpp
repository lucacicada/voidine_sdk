#include "rollback_tree.h"
#include "network.h"
#include "rollback_multiplayer.h"

void RollbackTree::initialize() {
	SceneTree::initialize();

	Network::get_singleton()->reset_time();

	Network::get_singleton()->_in_rollback = false;
	Network::get_singleton()->_network_frames = 0;
}

// deterministic lockstep with rollover
int RollbackTree::get_override_physics_steps() {
	if (offline_and_sad) {
		return -1; // default behavior, no rollback, we are sad :(
	}

	// someone have ask to reset the simulation, do it
	if (time_reset_requested) {
		Network::get_singleton()->reset_time();
		time_reset_requested = false;
	}

	const int max_physics_steps = Engine::get_singleton()->get_max_physics_steps_per_frame();
	const int physics_ticks_per_second = Engine::get_singleton()->get_physics_ticks_per_second();
	const double physics_step = 1.0 / physics_ticks_per_second;

	Network::get_singleton()->_simulation_clock_ptr->step();
	Network::get_singleton()->_simulation_clock_ptr->advance(physics_step, max_physics_steps);

	// pretend the simulation run faster/slower to catch up
	Network::get_singleton()->_simulation_clock_ptr->stretch_towards(Network::get_singleton()->_reference_clock_ptr->get_time(), physics_step);

	return Network::get_singleton()->_simulation_clock_ptr->steps;
}

// before physic sync, in_physics = true
void RollbackTree::iteration_prepare() {
	SceneTree::iteration_prepare();

	// same as Godot, increment a tick BEFORE executing the actual step, not afterwards
	// network frames increases by a fixed delta, regardless of wall time
	Network::get_singleton()->_in_rollback = false;
	Network::get_singleton()->_network_frames++;
}

// in full physics sync, in_physics = true
// RollbackTree::physics_process // emit_signal(SNAME("network_frame"));

// after physic sync, in_physics = true
void RollbackTree::iteration_end() {
	SceneTree::iteration_end();
}

// after physic loop, in_physics = false
bool RollbackTree::process(double p_time) {
	return SceneTree::process(p_time); // process() will poll() multiplayer APIs if "multiplayer_poll" is true
}

void RollbackTree::_bind_methods() {
}

RollbackTree::RollbackTree() {
	if (singleton == nullptr) {
		singleton = this;
	}

	// if (SceneTree::get_multiplayer()->derives_from<RollbackMultiplayer>()) {
	if (SceneTree::get_multiplayer()->is_class("RollbackMultiplayer")) {
		// SceneTree::get_multiplayer()->connect(SNAME("connected_to_server"), callable_mp(this, &RollbackTree::_connected_to_server));
		// SceneTree::get_multiplayer()->connect(SNAME("server_disconnected"), callable_mp(this, &RollbackTree::_server_disconnected));
	} else {
		offline_and_sad = true;

#ifdef TOOLS_ENABLED
		WARN_PRINT_ED("RollbackTree requires a RollbackMultiplayer instance as the root multiplayer API, this message is only shown in the editor.");
#endif
	}
}

RollbackTree::~RollbackTree() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
