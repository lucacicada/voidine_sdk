#include "network.h"

Network *Network::get_singleton() {
	return singleton;
}

void Network::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_in_rollback_frame"), &Network::is_in_rollback_frame);
	ClassDB::bind_method(D_METHOD("get_network_frames"), &Network::get_network_frames);
	ClassDB::bind_method(D_METHOD("reset"), &Network::reset);
}

Network::Network() {
	ERR_FAIL_COND_MSG(singleton, "Singleton for Network already exists.");
	singleton = this;
}

Network::~Network() {
	singleton = nullptr;
}
