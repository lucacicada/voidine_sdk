#pragma once

#include "network.h"

#include "core/os/os.h"
#include "core/os/thread_safe.h"
#include "rollback_synchronizer.h"
#include "scene/main/scene_tree.h"

class RollbackTree : public SceneTree {
	_THREAD_SAFE_CLASS_

	GDCLASS(RollbackTree, SceneTree);

private:
	friend class RollbackMultiplayer;

	// learn about network time
	// clocks are in fraction seconds (double) instead of uint64_t
	// fraction seconds are easier to work with, they maintain solid precision for a very long time
	// double has 53 bits of mantissa (precision) (significand)

	static double get_raw_time() {
		return OS::get_singleton()->get_ticks_usec() / 1000000.0;
	}

	struct NetworkClock {
		double offset = 0;

		double get_time() const {
			return get_raw_time() + offset;
		}

		void set_time(double p_time) {
			offset = p_time - get_raw_time();
		}

		void adjust(double p_offset) {
			offset += p_offset;
		}
	};

	struct SteppingClock {
		double time = 0;
		double last_time = 0;

		double get_time() const {
			return time;
		}

		void set_time(double p_time) {
			last_time = get_raw_time();
			time = p_time;
		}

		void adjust(double p_offset) {
			time += p_offset;
		}

		void step(double p_multiplier = 1.0) {
			const double current_step = get_raw_time();
			const double step_duration = current_step - last_time;
			last_time = current_step;

			adjust(step_duration * p_multiplier);
		}
	};

	inline static RollbackTree *singleton = nullptr;

	NetworkClock network_clock;
	SteppingClock stepping_clock;

	bool offline_and_sad = false;

	double integral_error = 0.0;
	double clock_stretch_factor = 1.0;
	double accumulator = 0.0;

	Vector<RollbackSynchronizer *> synchronizers;

	void adjust_network_clock(double p_offset) {
		network_clock.adjust(p_offset);
	}
	double get_network_clock_time() const {
		return network_clock.get_time();
	}
	void set_network_clock_unsafe_timestamp(double p_time) { //TODO: this is so bad, force set the initial timestamp
		network_clock.set_time(p_time);

		const double ts = network_clock.get_time();
		Network::get_singleton()->_network_frames = uint64_t(ts * Engine::get_singleton()->get_physics_ticks_per_second());
	}

protected:
	static void _bind_methods();

public:
	static RollbackTree *get_singleton() { return singleton; }

	virtual int get_override_physics_steps() override;
	virtual void initialize() override;
	virtual void iteration_prepare() override;
	virtual void iteration_end() override;
	virtual bool process(double p_time) override;

	void add_synchronizer(RollbackSynchronizer *p_synchronizer) {
		ERR_FAIL_NULL(p_synchronizer);

		synchronizers.push_back(p_synchronizer);
	}
	void remove_synchronizer(RollbackSynchronizer *p_synchronizer) {
		ERR_FAIL_NULL(p_synchronizer);

		synchronizers.erase(p_synchronizer);
	}

	RollbackTree();
	~RollbackTree();
};
