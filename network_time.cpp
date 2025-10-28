#include "network_time.h"

NetworkTime *NetworkTime::get_singleton() {
	return singleton;
}

double NetworkTime::get_time_seconds() const {
	return double(get_ticks_usec()) / 1000000.0;
}

uint64_t NetworkTime::get_ticks_msec() const {
	return get_ticks_usec() / 1000ULL;
}

uint64_t NetworkTime::get_ticks_usec() const {
	return int64_t(OS::get_singleton()->get_ticks_usec()) + offset;
}

void NetworkTime::set_time(int64_t p_time) {
	offset = p_time - int64_t(OS::get_singleton()->get_ticks_usec());
}

void NetworkTime::set_offset(int64_t p_offset) {
	offset = p_offset;
}

// Move the network time by the given offset in microseconds.
// Unbounded, if you pass in a very large offset, the game becomes patrick star.
void NetworkTime::adjust_time(int64_t p_offset) {
	// // Check for int64_t overflow
	// if ((p_offset > 0 && offset > INT64_MAX - p_offset) || (p_offset < 0 && offset < INT64_MIN - p_offset)) { return; }
	// // Check for double precision loss 53 bits of significand
	// int64_t new_offset = offset + p_offset;
	// if (new_offset > (1LL << 53) || new_offset < -(1LL << 53)) { return; }

	offset += p_offset;
}

void NetworkTime::_bind_methods() {
}

NetworkTime::NetworkTime() {
	ERR_FAIL_COND_MSG(singleton, "Singleton for NetworkTime already exists.");
	singleton = this;
}

NetworkTime::~NetworkTime() {
	singleton = nullptr;
}
