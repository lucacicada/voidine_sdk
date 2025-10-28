#include "rollback_tree.h"
#include "network.h"
#include "rollback_multiplayer.h"

void RollbackTree::initialize() {
	SceneTree::initialize();

	// if we do not initialize the clocks well, the game will be sluggish like crazy
	const double now = get_raw_time();
	network_clock.offset = 0;
	stepping_clock.last_time = now;
	stepping_clock.time = now;
	accumulator = now;
}

// deterministic lockstep with rollover
int RollbackTree::get_override_physics_steps() {
	if (offline_and_sad) {
		return -1; // default behavior, no rollback, we are sad :(
	}

	const int max_physics_steps = Engine::get_singleton()->get_max_physics_steps_per_frame();
	const int physics_ticks_per_second = Engine::get_singleton()->get_physics_ticks_per_second();
	const double physics_step = 1.0 / physics_ticks_per_second;

	const double now = get_raw_time();

	// lockstep clock
	{
		const double delta = now - stepping_clock.last_time;
		stepping_clock.last_time = now;
		stepping_clock.time += delta * clock_stretch_factor;
	}

	// linear smoothing
	// {
	// 	const double clock_diff = (now + network_clock.offset) - stepping_clock.time;

	// 	const double gain = 0.01;
	// 	const double min_diff = 0.001;
	// 	const double clock_stretch_max = 1.05;
	// 	const double clock_stretch_min = 1.0 / 1.05;
	// 	const double smoothing_alpha = 0.1;

	// 	const double target_stretch = CLAMP(1.0 + gain * clock_diff, clock_stretch_min, clock_stretch_max);
	// 	const double mask = double(std::abs(clock_diff) >= min_diff);

	// 	clock_stretch_factor = Math::lerp(clock_stretch_factor, target_stretch, smoothing_alpha * mask);
	// }

	// proportional-integral clock adjustment
	// reference: https://en.wikipedia.org/wiki/Proportional-integral-derivative_controller
	{
		const double Kp = 0.1;
		const double Ki = 0.01;

		const double clock_stretch_max = 1.05;
		const double clock_stretch_min = 1.0 / 1.05;

		const double network_time = (now + network_clock.offset); // network_clock.get_time()
		const double error = network_time - stepping_clock.time;
		integral_error += error * physics_step; // dt

		double stretch = 1.0 + Kp * error + Ki * integral_error;
		stretch = CLAMP(stretch, clock_stretch_min, clock_stretch_max);

		clock_stretch_factor = stretch;
	}

	int ticks_in_loop = 0;
	while (accumulator < stepping_clock.time && ticks_in_loop < max_physics_steps) {
		accumulator += physics_step;
		++ticks_in_loop;
	}
	return ticks_in_loop;
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
