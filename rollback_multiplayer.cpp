#include "rollback_multiplayer.h"
#include "network.h"
#include "network_input.h"
#include "rollback_tree.h"

#include "modules/enet/enet_multiplayer_peer.h"

#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

bool RollbackMultiplayer::is_online_server() const {
	return rollback_state == ROLLBACK_STATE_SERVER;
}

void RollbackMultiplayer::set_multiplayer_peer(const Ref<MultiplayerPeer> &p_peer) {
	SceneMultiplayer::set_multiplayer_peer(p_peer);
	_update_rollback_state();
}

Error RollbackMultiplayer::object_configuration_add(Object *p_obj, Variant p_config) {
	// if (p_obj == nullptr && p_config.get_type() == Variant::NODE_PATH) {
	// 	if (SceneTree::get_singleton()->get_multiplayer() == this) {
	// 		const NodePath tree_root_path = NodePath("/" + SceneTree::get_singleton()->get_root()->get_name());
	// 		if (p_config == tree_root_path) {
	// 			// we are getting set to the root tree, start the simulation
	// 		}
	// 	}
	// }

	if (p_obj != nullptr) {
		NetworkInput *input = Object::cast_to<NetworkInput>(p_obj);
		if (input != nullptr) {
			if (is_server()) {
				// on the server track all inputs
				return input_replication->add_input(input);
			}
			if (input->is_multiplayer_authority()) {
				// on the client, only add inputs that are authoritative (local)
				return input_replication->add_input(input);
			}
			// the error is unused by the engine and is mainly for override purposes
			// in this case untracked inputs are not an error but intended behavior
			return OK;
		}
	}

	return SceneMultiplayer::object_configuration_add(p_obj, p_config);
}

Error RollbackMultiplayer::object_configuration_remove(Object *p_obj, Variant p_config) {
	// if (p_obj == nullptr && p_config.get_type() == Variant::NODE_PATH) {
	// 	if (SceneTree::get_singleton()->get_multiplayer() == this) {
	// 		const NodePath tree_root_path = NodePath("/" + SceneTree::get_singleton()->get_root()->get_name());
	// 		if (p_config == tree_root_path) {
	// 			// we are getting removed from the root tree, end the simulation
	// 		}
	// 	}
	// }

	if (p_obj != nullptr) {
		NetworkInput *input = Object::cast_to<NetworkInput>(p_obj);
		if (input != nullptr) {
			return input_replication->remove_input(input);
		}
	}

	return SceneMultiplayer::object_configuration_remove(p_obj, p_config);
}

// poll can be executed manually by the user, or automatically in SceneTree before the process step (see SceneTree::"process_frame" signal)
// if multiplayer_poll is set to true (default)
// the Error appear to be unused
Error RollbackMultiplayer::poll() {
	Error err = SceneMultiplayer::poll();
	_update_rollback_state();

	if (rollback_state == ROLLBACK_STATE_CLIENT) {
		if (ping_timer.elapsed()) {
			err = ping();
		}
	}

	return err;
}

Error RollbackMultiplayer::ping() {
	ERR_FAIL_COND_V_MSG(get_unique_id() == 1, ERR_UNAVAILABLE, "Server cannot send ping to itself.");
	ERR_FAIL_COND_V_MSG(!SceneMultiplayer::get_connected_peers().has(MultiplayerPeer::TARGET_PEER_SERVER), ERR_UNAVAILABLE, "Cannot send ping to server when not connected.");

	// get a new sample
	++sample_index;
	PingSample sample;
	sample.ping_sent = Network::get_singleton()->_reference_clock_ptr->get_time();
	awaiting_samples[sample_index] = sample;

	Vector<uint8_t> packet;
	packet.resize(6);
	packet.write[0] = SceneMultiplayer::NETWORK_COMMAND_RAW | CMD_FLAG_PING_PONG_SHIFT;
	packet.write[1] = COMMAND_PING;
	encode_uint32(sample_index, &packet.write[2]);

	Ref<MultiplayerPeer> peer = get_multiplayer_peer();
	ERR_FAIL_COND_V(peer.is_null(), ERR_UNAVAILABLE);

	peer->set_transfer_channel(0);
	peer->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_UNRELIABLE);
	return send_command(MultiplayerPeer::TARGET_PEER_SERVER, packet.ptr(), packet.size());
}

void RollbackMultiplayer::_process_packet(int p_from, const uint8_t *p_packet, int p_packet_len) {
	if (p_packet_len > 0) {
		const uint8_t packet_type = p_packet[0] & CMD_MASK;
		if (packet_type == NETWORK_COMMAND_RAW) {
			if ((p_packet[0] & (1 << CMD_FLAG_1_SHIFT)) != 0) {
				input_replication->process_inputs(p_from, p_packet, p_packet_len);
			} else {
				const bool is_ping_mask = (p_packet[0] & (CMD_FLAG_PING_PONG_SHIFT)) != 0;
				if (is_ping_mask && p_packet_len > 1) {
					ERR_FAIL_COND_MSG(p_packet_len < 2, "Invalid packet received. Size too small.");

					const bool is_ping = p_packet[1] == COMMAND_PING;
					if (is_ping) {
						_process_ping(p_from, &p_packet[2], p_packet_len - 2);
					} else if (p_packet[1] == COMMAND_PONG) {
						_process_pong(p_from, &p_packet[2], p_packet_len - 2);
					}
					return;
				}
			}
		}
	}

	SceneMultiplayer::_process_packet(p_from, p_packet, p_packet_len);
}

void RollbackMultiplayer::_process_ping(int p_from, const uint8_t *p_packet, int p_packet_len) {
	ERR_FAIL_COND_MSG(p_packet_len < 4, "Invalid ping packet received. Size too small.");
	const uint32_t idx = decode_uint32(&p_packet[0]);

	double last_rtt = 0;

	Ref<MultiplayerPeer> peer = get_multiplayer_peer();
	ENetMultiplayerPeer *enet = Object::cast_to<ENetMultiplayerPeer>(peer.ptr());
	if (enet) {
		Ref<ENetPacketPeer> peer_packet = enet->get_peer(p_from);
		if (peer_packet.is_valid() && peer_packet->is_active()) {
			last_rtt = peer_packet->get_statistic(ENetPacketPeer::PEER_LAST_ROUND_TRIP_TIME);

			// CORRECTION PACKET STATS: send them to erratic clients to help them adjust better
			// peer_packet->get_statistic(ENetPacketPeer::PEER_ROUND_TRIP_TIME); // Useful for smoothing and predicting network latency trends.
			// peer_packet->get_statistic(ENetPacketPeer::PEER_ROUND_TRIP_TIME_VARIANCE); // Helps the client understand jitter and how stable the connection is.

			// DIAGNOSTIC ONLY: only send them if diagnostic is enabled
			// peer_packet->get_statistic(ENetPacketPeer::PEER_PACKET_LOSS); // Helps the client decide if the connection is unreliable.
			// peer_packet->get_statistic(ENetPacketPeer::PEER_PACKET_LOSS_VARIANCE); // Shows how bursty or stable packet loss is.
			// peer_packet->get_statistic(ENetPacketPeer::PEER_PACKET_THROTTLE); // Helps the client understand how ENet is adapting to network conditions.
		}
	}

	Vector<uint8_t> packet;
	packet.resize(24);
	packet.write[0] = SceneMultiplayer::NETWORK_COMMAND_RAW | CMD_FLAG_PING_PONG_SHIFT;
	packet.write[1] = COMMAND_PONG;

	{
		uint8_t *w = &packet.write[2];
		memcpy(&w[0], &p_packet[0], 4); // copy back the idx

		const double time = Network::get_singleton()->_reference_clock_ptr->get_time();
		const uint64_t ticks = Network::get_singleton()->_network_frames;

		encode_double(time, &w[4]); // server clock raw
		encode_uint64(ticks, &w[12]); // server tick
		encode_half(last_rtt, &w[20]); // Quantized last RTT
	}

	send_command(p_from, packet.ptr(), packet.size());
}

void RollbackMultiplayer::_process_pong(int p_from, const uint8_t *p_packet, int p_packet_len) {
	ERR_FAIL_COND_MSG(p_packet_len < 20, "Invalid pong packet received. Size too small.");

	const uint32_t idx = decode_uint32(&p_packet[0]);

	if (!awaiting_samples.has(idx)) {
		return; // packet drop somewhere
	}

	const double server_clock = decode_double(&p_packet[4]); // TODO: Validate server clock, it should be in some boundary
	const uint64_t server_tick = decode_uint64(&p_packet[12]);
	const double last_rtt = p_packet_len >= 22 ? decode_half(&p_packet[20]) : 0;

	const double pong_received = Network::get_singleton()->_reference_clock_ptr->get_time();

	PingSample sample = awaiting_samples[idx];
	awaiting_samples.erase(idx);

	sample.ping_received = server_clock;
	sample.pong_sent = server_clock;
	sample.pong_received = pong_received;

	if (sample_buffer.size() == 0) {
		Network::get_singleton()->_reference_clock_ptr->set_time(server_clock);
		Network::get_singleton()->_simulation_clock_ptr->set_time(Network::get_singleton()->_reference_clock_ptr->get_time());
		Network::get_singleton()->_network_frames = Network::get_singleton()->_reference_clock_ptr->get_reference_frames();
		Network::get_singleton()->_network_frames = server_tick; // TODO: do something here
	}

	sample_buffer.append(sample);

	_adjust_clock();
}

void RollbackMultiplayer::_adjust_clock() {
	Vector<PingSample> sorted_samples = sample_buffer.sort_custom<PingSample::PingSampleSorter>();

	ERR_FAIL_COND(sorted_samples.size() == 0); // should never happen

	const double rtt_min = sorted_samples[0].get_rtt();
	const double rtt_max = sorted_samples[sorted_samples.size() - 1].get_rtt();

	const double _rtt = (rtt_max + rtt_min) / 2;
	const double _rtt_jitter = (rtt_max - rtt_min) / 2;

	double offset = 0;
	double offset_weight = 0;
	for (uint32_t i = 0; i < sorted_samples.size(); ++i) {
		const double rtt = sorted_samples[i].get_rtt();
		const double sample_offset = sorted_samples[i].get_offset();

		const double weight = log(1.0 + double(rtt));
		offset += sample_offset * weight;
		offset_weight += weight;
	}

	if (Math::is_zero_approx(offset_weight)) {
		offset /= double(sorted_samples.size());
	} else {
		offset /= offset_weight;
	}

	const double panic_threshold = 2.0; // seconds
	const int adjust_steps = 8;

	if (Math::abs(offset) > panic_threshold) {
		// Network::get_singleton()->_reference_clock_ptr->adjust(offset);
		awaiting_samples.clear();
	} else {
		const double nudge = offset / double(adjust_steps);
		Network::get_singleton()->_reference_clock_ptr->adjust(nudge);
	}
}

void RollbackMultiplayer::before_physic_process() {
	// gather inputs from registered NetworkInput nodes
	input_replication->gather_inputs();
}

void RollbackMultiplayer::_bind_methods() {
}

RollbackMultiplayer::RollbackMultiplayer() {
	input_replication.instantiate(this);
}

RollbackMultiplayer::~RollbackMultiplayer() {
	input_replication.unref();
}

void RollbackMultiplayer::_update_rollback_state() {
	Ref<MultiplayerPeer> peer = SceneMultiplayer::get_multiplayer_peer();
	MultiplayerPeer::ConnectionStatus status = peer.is_valid() ? peer->get_connection_status() : MultiplayerPeer::CONNECTION_DISCONNECTED;

	if (status == MultiplayerPeer::CONNECTION_CONNECTED) {
		if (peer->get_unique_id() == MultiplayerPeer::TARGET_PEER_SERVER) {
			// a peer that cannot send data is offline
			if (peer->get_max_packet_size() == 0) { // peer->is_class("OfflineMultiplayerPeer")
				rollback_state = ROLLBACK_STATE_OFFLINE;
			} else {
				rollback_state = ROLLBACK_STATE_SERVER;
			}
		}
		// we must check for connected peers here
		// during authentication phase, we are "connected" but not authenticated, we cant's send messages
		// "connected_to_server" signal is emitted when we are fully authenticated
		else if (SceneMultiplayer::get_connected_peers().has(MultiplayerPeer::TARGET_PEER_SERVER)) {
			rollback_state = ROLLBACK_STATE_CLIENT;
		} else {
			rollback_state = ROLLBACK_STATE_OFFLINE; // connected but not authenticated
		}
	} else {
		rollback_state = ROLLBACK_STATE_OFFLINE;
	}

	last_rollback_state = rollback_state;
}
