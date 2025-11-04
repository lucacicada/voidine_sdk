#pragma once

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/os/os.h"

static double get_wall_time() {
	return OS::get_singleton()->get_ticks_usec() / 1000000.0;
}

// about network time:
// clocks are in fraction seconds (double) instead of uint64_t
// fraction seconds are easier to work with, they maintain solid precision for a very long time
// double has 53 bits of significand precision

class ReferenceClock : public Object {
	GDCLASS(ReferenceClock, Object);

private:
	friend class Network;
	friend class RollbackTree;

	double offset = 0;

protected:
	static void _bind_methods();

public:
	double get_time() const {
		return get_wall_time() + offset;
	}
	void set_time(double p_time) {
		offset = p_time - get_wall_time();
	}
	void set_offset(double p_offset) {
		offset = p_offset;
	}
	void adjust(double p_offset) {
		offset += p_offset;
	}

	uint64_t get_reference_frames() const { //TODO: move outside the clock, clocks runs independantly of any external settings
		return uint64_t(get_time() * Engine::get_singleton()->get_physics_ticks_per_second());
	}
};

class SimulationClock : public Object {
	GDCLASS(SimulationClock, Object);

private:
	friend class Network;
	friend class RollbackTree;

	double time = 0;
	double last_time = 0;
	double time_scale = 1.0;
	double accumulator = 0;
	double integral_error = 0; // correct stretch over time
	int steps = 0; // remeber how many steps we are currently taking

protected:
	static void _bind_methods();

public:
	double get_time() const {
		return time;
	}
	void set_time(double p_time) {
		last_time = get_wall_time();
		time = p_time;
		accumulator = p_time;
	}
	void set_step_scale(double p_time_scale) {
		time_scale = p_time_scale;
	}
	double get_step_scale() const {
		return time_scale;
	}
	void adjust(double p_offset) {
		time += p_offset;
	}
	uint64_t get_simulated_frames() const { //TODO: move outside the clock, clocks runs independantly of any external settings
		return uint64_t(get_time() * Engine::get_singleton()->get_physics_ticks_per_second());
	}

	void step() {
		const double current_step = get_wall_time();
		const double step_duration = current_step - last_time;
		last_time = current_step;
		adjust(step_duration * time_scale);
	}

	void adjust_towards(double p_time, double p_step) {
		const double Kp = 0.1;
		const double Ki = 0.01;

		const double clock_stretch_max = 1.05;
		const double clock_stretch_min = 1.0 / 1.05;

		const double error = p_time - time;
		integral_error += error * p_step; // dt

		const double integral_max = 10.0;
		integral_error = CLAMP(integral_error, -integral_max, integral_max);

		double stretch = 1.0 + Kp * error + Ki * integral_error;
		stretch = CLAMP(stretch, clock_stretch_min, clock_stretch_max);

		time_scale = stretch;
	}

	int advance(double p_step, int p_max_steps) {
		int i = 0;
		while (accumulator < time && i < p_max_steps) {
			accumulator += p_step;
			++i;
		}
		steps = i;
		return i;
	}
};

class Network : public Object {
	GDCLASS(Network, Object);

private:
	friend class RollbackTree; // RollbackTree have direct access
	friend class RollbackMultiplayer; // RollbackTree have direct access

	inline static Network *singleton = nullptr;

private:
	ReferenceClock *_reference_clock_ptr = nullptr;
	SimulationClock *_simulation_clock_ptr = nullptr;

	void reset_time() {
		// _reference_clock_ptr->set_time(0);
		// _simulation_clock_ptr->set_time(Network::get_singleton()->_reference_clock_ptr->get_time());

		const double n = get_wall_time();
		_reference_clock_ptr->offset = -n;
		_simulation_clock_ptr->last_time = n;
		_simulation_clock_ptr->time = 0.0;
		_simulation_clock_ptr->accumulator = 0.0;

		_simulation_clock_ptr->time_scale = 1.0;
		_simulation_clock_ptr->integral_error = 0.0;
		_simulation_clock_ptr->steps = 0;

		_network_frames = 0;
	}

	bool _in_rollback = false; // if the current frame is a rollback frame
	uint64_t _network_frames = 0; // frame elapsed since start, increments by 1 each physics frame

protected:
	static void _bind_methods();

public:
	static Network *get_singleton();

	ReferenceClock *get_reference_clock() const { return _reference_clock_ptr; }
	SimulationClock *get_simulation_clock() const { return _simulation_clock_ptr; }

	bool is_in_rollback_frame() const { return _in_rollback; }

	// Current network time in frames.
	uint64_t get_network_frames() const { return _network_frames; } // TODO: rename, network frames have nothing to do with game ticks
	uint64_t get_simulation_frames() const { return _network_frames; }
	uint64_t get_tick() const { return _network_frames; }
	int get_steps_count() const { return _simulation_clock_ptr->steps; }

	Network();
	virtual ~Network();
};
