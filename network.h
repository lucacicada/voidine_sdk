#pragma once

#include "core/config/engine.h"
#include "core/object/class_db.h"

class Network : public Object {
	GDCLASS(Network, Object);

private:
	friend class RollbackTree; // RollbackTree have direct access

	inline static Network *singleton = nullptr;

	bool _in_rollback = false;
	uint64_t _network_frames = 0; // frame elapsed since start, increments by 1 each physics frame

protected:
	static void _bind_methods();

public:
	static Network *get_singleton();

	bool is_in_rollback_frame() const { return _in_rollback; }

	// Current network time in microseconds.
	uint64_t get_network_ticks_usec() const { return (_network_frames * 1'000'000) / Engine::get_singleton()->get_physics_ticks_per_second(); }
	double get_network_time() const { return _network_frames / double(Engine::get_singleton()->get_physics_ticks_per_second()); }

	// Current network time in frames.
	uint64_t get_network_frames() const { return _network_frames; }
	uint64_t get_tick() const { return _network_frames; }

	// for debugging/testing purposes
	void reset() { _network_frames = 0; }

	// Now many network ticks per second (same as physics ticks per second).
	uint64_t get_network_ticks_per_second() const { return Engine::get_singleton()->get_physics_ticks_per_second(); }

	// TODO: move to NetworkTime singleton
	uint64_t get_ticks_msec() const { return (_network_frames * 1'000) / Engine::get_singleton()->get_physics_ticks_per_second(); }
	uint64_t get_ticks_usec() const { return (_network_frames * 1'000'000) / Engine::get_singleton()->get_physics_ticks_per_second(); }

	Network();
	virtual ~Network();
};
