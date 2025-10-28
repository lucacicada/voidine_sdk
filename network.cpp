#include "network.h"

void ReferenceClock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_time"), &ReferenceClock::get_time);
	ClassDB::bind_method(D_METHOD("get_reference_frames"), &ReferenceClock::get_reference_frames);
}

void SimulationClock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_time"), &SimulationClock::get_time);
	ClassDB::bind_method(D_METHOD("get_stretch_factor"), &SimulationClock::get_stretch_factor);
}

Network *Network::get_singleton() {
	return singleton;
}

void Network::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_in_rollback_frame"), &Network::is_in_rollback_frame);
	ClassDB::bind_method(D_METHOD("get_network_frames"), &Network::get_network_frames);
	ClassDB::bind_method(D_METHOD("reset"), &Network::reset);
	ClassDB::bind_method(D_METHOD("get_system_time"), &Network::get_system_time);

	ClassDB::bind_method(D_METHOD("get_reference_clock"), &Network::get_reference_clock);
	ClassDB::bind_method(D_METHOD("get_simulation_clock"), &Network::get_simulation_clock);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "reference_clock"), "", "get_reference_clock");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "simulation_clock"), "", "get_simulation_clock");
}

Network::Network() {
	ERR_FAIL_COND_MSG(singleton, "Singleton for Network already exists.");
	singleton = this;

	_reference_clock_ptr = memnew(ReferenceClock);
	_simulation_clock_ptr = memnew(SimulationClock);
}

Network::~Network() {
	singleton = nullptr;
}
