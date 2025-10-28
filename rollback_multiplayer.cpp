#include "rollback_multiplayer.h"
#include "network.h"
#include "rollback_tree.h"

#include "core/io/marshalls.h"
#include "core/os/os.h"

// poll can be executed manually by the user, or automatically in SceneTree before the process step (see SceneTree::"process_frame" signal)
// if multiplayer_poll is set to true (default)
// the Error appear to be unused
Error RollbackMultiplayer::poll() {
	Error err = SceneMultiplayer::poll();
	_update_state();

	if (multiplayer_state == STATE_CLIENT) {
		if (ping_timer.elapsed()) {
			++sample_index;
			PingSample sample;
			sample.ping_sent = get_network_time();
			awaiting_samples[sample_index] = sample;

			err = send_ping_to_server(sample_index);
		}
	}

	return err;
}

Error RollbackMultiplayer::send_ping_to_server(int p_ping_request_index) {
	ERR_FAIL_COND_V(get_unique_id() == 1, ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(!SceneMultiplayer::get_connected_peers().has(MultiplayerPeer::TARGET_PEER_SERVER), ERR_UNAVAILABLE);

	Vector<uint8_t> out;
	out.resize(1 + 4); // packet_type + idx

	{
		uint8_t *w = out.ptrw();
		w[0] = CUSTOM_COMMAND_PING;
		encode_uint32(p_ping_request_index, &w[1]);
	}

	return send_bytes(out, MultiplayerPeer::TARGET_PEER_SERVER, MultiplayerPeer::TRANSFER_MODE_UNRELIABLE);
}

void RollbackMultiplayer::_peer_packet(int p_peer_id, const Vector<uint8_t> &p_packet) {
	if (get_unique_id() == 1 && p_peer_id != 1 && p_packet.size() > 1) { // Server side
		const uint8_t *r = p_packet.ptr();
		const uint8_t packet_type = r[0];
		if (packet_type == CUSTOM_COMMAND_PING) { // PACKET_TYPE_PING
			ERR_FAIL_COND(p_packet.size() != 1 + 4); // 1 plus 8 bytes idx

			Vector<uint8_t> out;
			out.resize(1 + 4 + 8);
			{
				const uint64_t ticks = get_network_time();

				uint8_t *w = out.ptrw();
				w[0] = CUSTOM_COMMAND_PONG;
				memcpy(&w[1], &r[1], 4); // copy back the idx
				encode_uint64(ticks, &w[1 + 4]); // server clock quantized
			}

			// // TODO: grab stats from server side to pass to client for better clock discipline
			// // client and server have different views on RTT and clock offset
			// MultiplayerPeer *p = get_multiplayer_peer().ptr();
			// if (p) {
			// 	ENetMultiplayerPeer *enet_peer = Object::cast_to<ENetMultiplayerPeer>(p);
			// 	if (enet_peer) {
			// 		ENetPacketPeer *enet_packet = enet_peer->get_peer(p_peer_id).ptr();
			// 		if (enet_packet) {
			// 			enet_packet->get_statistic(ENetPacketPeer::PeerStatistic::PEER_ROUND_TRIP_TIME);
			// 		}
			// 	}
			// }

			// send pong back to the peer
			send_bytes(out, p_peer_id, MultiplayerPeer::TRANSFER_MODE_UNRELIABLE);
		}
	}

	if (get_unique_id() != 1 && p_peer_id == 1 && p_packet.size() > 1) { // Client side
		const uint8_t *r = p_packet.ptr();
		const uint8_t packet_type = r[0];
		if (packet_type == CUSTOM_COMMAND_PONG) {
			ERR_FAIL_COND(p_packet.size() != 1 + 4 + 8); // 1 plus 8 bytes idx, 8 bytes clock, 8 bytes tick

			const uint32_t idx = decode_uint32(&r[1]);
			const uint64_t server_clock = decode_uint64(&r[1 + 4]); // TODO: Validate server clock, it should be in some boundary

			const uint64_t pong_received = get_network_time();

			if (!awaiting_samples.has(idx)) {
				return; // packet drop somewhere
			}

			PingSample sample = awaiting_samples[idx];
			awaiting_samples.erase(idx);

			sample.ping_received = server_clock;
			sample.pong_sent = server_clock;
			sample.pong_received = pong_received;

			if (sample_buffer.size() == 0) {
				_set_timestamp(server_clock);
			}

			sample_buffer.append(sample);
			_adjust_clock();
		}
	}
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
		adjust_network_clock(offset);
		// sorted_samples.clear();
		awaiting_samples.clear();
	} else {
		const double nudge = offset / double(adjust_steps);

		// TODO: THIS IS A BAD IDEA, do not call adjust directly from here
		adjust_network_clock(nudge);
	}
}

void RollbackMultiplayer::_bind_methods() {
}

RollbackMultiplayer::RollbackMultiplayer() {
	connect(SNAME("peer_packet"), callable_mp(this, &RollbackMultiplayer::_peer_packet));
}

void RollbackMultiplayer::_update_state() {
	Ref<MultiplayerPeer> peer = get_multiplayer_peer();

	if (peer.is_valid() && peer->get_connection_status() == MultiplayerPeer::CONNECTION_CONNECTED) {
		if (peer->get_unique_id() == MultiplayerPeer::TARGET_PEER_SERVER) {
			multiplayer_state = STATE_SERVER;
		}
		// we must check for connected peers here
		// during authentication phase, we are "connected" but not authenticated yed
		// "connected_to_server" signal is emitted when we are fully authenticated
		else if (get_connected_peers().has(MultiplayerPeer::TARGET_PEER_SERVER)) {
			multiplayer_state = STATE_CLIENT;
		} else {
			multiplayer_state = STATE_OFFLINE; // connected but not authenticated
		}
	} else {
		multiplayer_state = STATE_OFFLINE;
	}
}
