#include "rollback_tree.h"
#include "network.h"
#include "rollback_multiplayer.h"

void RollbackTree::initialize() {
	SceneTree::initialize();

	// if we do not initialize the clocks well, the game will be sluggish like crazy
	double now = get_raw_time();
	network_clock.offset = 0;
	stepping_clock.last_time = now;
	stepping_clock.time = now;
	accumulator = now;

	now = get_wall_time();
	Network::get_singleton()->_simulation_clock_ptr->time = now;
	Network::get_singleton()->_simulation_clock_ptr->last_time = now;
	Network::get_singleton()->_simulation_clock_ptr->accumulator = 0;
	Network::get_singleton()->_reference_clock_ptr->offset = 0;

	// Network::get_singleton()->_reference_clock.set_offset(0);
	// Network::get_singleton()->_simulation_clock.set_time(Network::get_singleton()->_reference_clock.get_time());

	Network::get_singleton()->_reference_clock_ptr->set_time(0);
	Network::get_singleton()->_simulation_clock_ptr->set_time(Network::get_singleton()->_reference_clock_ptr->get_time());

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

	RollbackMultiplayer *rollback_multiplayer = Object::cast_to<RollbackMultiplayer>(get_multiplayer().ptr());
	if (!rollback_multiplayer) {
		offline_and_sad = true;
#ifdef TOOLS_ENABLED
		WARN_PRINT_ED("RollbackTree requires a RollbackMultiplayer instance as the root multiplayer API, this message is only shown in the editor.");
#endif
	} else {
		// rollback_multiplayer->connect(SNAME("connected_to_server"), callable_mp(this, &RollbackTree::_connected_to_server));
		// rollback_multiplayer->connect(SNAME("server_disconnected"), callable_mp(this, &RollbackTree::_server_disconnected));
	}
}

RollbackTree::~RollbackTree() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
