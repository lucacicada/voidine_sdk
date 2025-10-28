#pragma once

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/os/os.h"

static double get_wall_time() {
	return OS::get_singleton()->get_ticks_usec() / 1000000.0;
}

class ReferenceClock : public Object {
	GDCLASS(ReferenceClock, Object);

private:
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

	uint64_t get_reference_frames() const {
		return uint64_t(get_time() * Engine::get_singleton()->get_physics_ticks_per_second());
	}
};

class SimulationClock : public Object {
	GDCLASS(SimulationClock, Object);

private:
	friend class RollbackTree;

	double time = 0;
	double last_time = 0;
	double stretch_factor = 1.0;
	double accumulator = 0;
	double integral_error = 0; // correct stretch over time
	int steps = 0; // remmeber how many steps we are currently taking

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
	void set_stretch_factor(double p_stretch_factor) {
		stretch_factor = p_stretch_factor;
	}
	double get_stretch_factor() const {
		return stretch_factor;
	}
	void adjust(double p_offset) {
		time += p_offset;
	}
	void step() {
		const double current_step = get_wall_time();
		const double step_duration = current_step - last_time;
		last_time = current_step;
		adjust(step_duration * stretch_factor);
	}

	void stretch_towards(double p_time, double p_step) {
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

		stretch_factor = stretch;
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

	ReferenceClock _reference_clock;
	SimulationClock _simulation_clock;

	bool _in_rollback = false; // if the current frame is a rollback frame
	uint64_t _network_frames = 0; // frame elapsed since start, increments by 1 each physics frame

protected:
	static void _bind_methods();

public:
	static Network *get_singleton();

	ReferenceClock *get_reference_clock() const { return _reference_clock_ptr; }
	SimulationClock *get_simulation_clock() const { return _simulation_clock_ptr; }

	//
	double get_system_time() const { return _reference_clock.get_time(); }

	bool is_in_rollback_frame() const { return _in_rollback; }

	// Current network time in frames.
	uint64_t get_network_frames() const { return _network_frames; }
	uint64_t get_tick() const { return _network_frames; }

	// Current network time in microseconds.
	uint64_t get_network_ticks_usec() const { return (_network_frames * 1'000'000) / Engine::get_singleton()->get_physics_ticks_per_second(); }
	double get_network_time() const { return _network_frames / double(Engine::get_singleton()->get_physics_ticks_per_second()); }

	// for debugging/testing purposes
	void reset() {
		_network_frames = 0;
		_reference_clock.set_time(0);
	}

	// Now many network ticks per second (same as physics ticks per second).
	uint64_t get_network_ticks_per_second() const { return Engine::get_singleton()->get_physics_ticks_per_second(); }

	// TODO: move to NetworkTime singleton
	uint64_t get_ticks_msec() const { return (_network_frames * 1'000) / Engine::get_singleton()->get_physics_ticks_per_second(); }
	uint64_t get_ticks_usec() const { return (_network_frames * 1'000'000) / Engine::get_singleton()->get_physics_ticks_per_second(); }

	Network();
	virtual ~Network();
};
