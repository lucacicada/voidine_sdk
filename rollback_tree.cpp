#include "rollback_tree.h"
#include "network.h"
#include "network_actor.h"
#include "network_input.h"
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

	const int max_physics_steps = Engine::get_singleton()->get_max_physics_steps_per_frame();
	const int physics_ticks_per_second = Engine::get_singleton()->get_physics_ticks_per_second();
	const double physics_step = 1.0 / physics_ticks_per_second;

	Network::get_singleton()->_simulation_clock_ptr->step();
	Network::get_singleton()->_simulation_clock_ptr->advance(physics_step, max_physics_steps);

	// pretend the simulation run faster/slower to catch up
	Network::get_singleton()->_simulation_clock_ptr->adjust_towards(Network::get_singleton()->_reference_clock_ptr->get_time(), physics_step);

	return Network::get_singleton()->_simulation_clock_ptr->steps;
}

// before physic sync, in_physics = true
void RollbackTree::iteration_prepare() {
	SceneTree::iteration_prepare();

	// same as Godot, increment a tick BEFORE executing the actual step, not afterwards
	// network frames increases by a fixed delta, regardless of wall time
	Network::get_singleton()->_in_rollback = false;
	Network::get_singleton()->_network_frames++;

	// TODO: I do not like this cast here...
	RollbackMultiplayer *rm = Object::cast_to<RollbackMultiplayer>(get_multiplayer().ptr());
	if (rm) {
		rm->before_physic_process();
	}
}

// after physic sync, in_physics = true
void RollbackTree::iteration_end() {
	SceneTree::iteration_end();

	RollbackMultiplayer *rm = Object::cast_to<RollbackMultiplayer>(get_multiplayer().ptr());
	if (rm) {
		rm->after_physic_process();
	}
}

RollbackTree::RollbackTree() {
	if (singleton == nullptr) {
		singleton = this;
	}

	// if (SceneTree::get_multiplayer()->is_class("RollbackMultiplayer")) {
	if (SceneTree::get_multiplayer()->derives_from<RollbackMultiplayer>()) {
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
