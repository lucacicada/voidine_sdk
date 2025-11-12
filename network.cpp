#include "network.h"
#include "core/config/project_settings.h"

void ReferenceClock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_time"), &ReferenceClock::get_time);
	ClassDB::bind_method(D_METHOD("get_reference_frames"), &ReferenceClock::get_reference_frames);
}

void SimulationClock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_time"), &SimulationClock::get_time);
	ClassDB::bind_method(D_METHOD("get_time_scale"), &SimulationClock::get_step_scale);
	ClassDB::bind_method(D_METHOD("get_simulated_frames"), &SimulationClock::get_simulated_frames);
}

Network *Network::get_singleton() {
	return singleton;
}

void Network::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_in_rollback_frame"), &Network::is_in_rollback_frame);
	ClassDB::bind_method(D_METHOD("get_network_frames"), &Network::get_network_frames);

	ClassDB::bind_method(D_METHOD("get_reference_clock"), &Network::get_reference_clock);
	ClassDB::bind_method(D_METHOD("get_simulation_clock"), &Network::get_simulation_clock);

	ClassDB::bind_method(D_METHOD("get_steps_count"), &Network::get_steps_count);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "reference_clock"), "", "get_reference_clock");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "simulation_clock"), "", "get_simulation_clock");
}

Network::Network() {
	ERR_FAIL_COND_MSG(singleton, "Singleton for Network already exists.");
	singleton = this;

	_reference_clock_ptr = memnew(ReferenceClock);
	_simulation_clock_ptr = memnew(SimulationClock);

	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "multiplayer/common/network_ticks_per_second", PROPERTY_HINT_RANGE, "1,1000,1"), 60);
}

Network::~Network() {
	singleton = nullptr;

	memdelete(_reference_clock_ptr);
	memdelete(_simulation_clock_ptr);
}
