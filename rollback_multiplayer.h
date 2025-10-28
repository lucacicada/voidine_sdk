#pragma once

#include "rollback_tree.h"
#include "tinystuff.h"

#include "modules/multiplayer/scene_multiplayer.h"
#include "scene/main/multiplayer_api.h"

class RollbackMultiplayer : public SceneMultiplayer {
	GDCLASS(RollbackMultiplayer, SceneMultiplayer);

private:
	struct PingSample {
		struct PingSampleSorter {
			_ALWAYS_INLINE_ bool operator()(const PingSample &l, const PingSample &r) const {
				return l.get_rtt() < r.get_rtt();
			}
		};

		double ping_sent = 0;
		double ping_received = 0; // time at which the server have received the ping, same as pong_sent
		double pong_sent = 0;
		double pong_received = 0;

		double get_rtt() const {
			return (pong_received - ping_sent);
		}

		double get_offset() const {
			return ((ping_received - ping_sent) + (pong_sent - pong_received)) / 2.0;
		}
	};

	MicroTimer ping_timer = { 0.5 };

	uint32_t sample_index = 0;
	HashMap<uint32_t, PingSample> awaiting_samples;
	FixedBuffer<PingSample> sample_buffer{ 8 };
	void _adjust_clock();

	double get_network_time() const {
		// TODO: this circular calls are insanely bad
		return RollbackTree::get_singleton()->get_network_clock_time();
	}
	void adjust_network_clock(double p_offset) {
		// TODO: THIS IS A BAD IDEA, do not call adjust directly from here
		RollbackTree::get_singleton()->adjust_network_clock(p_offset);
	}
	void _set_timestamp(double p_time) {
		// TODO: now is so bad its even unsafe
		RollbackTree::get_singleton()->set_network_clock_unsafe_timestamp(p_time);
	}

	enum MultiplayerState {
		STATE_OFFLINE,
		STATE_SERVER,
		STATE_CLIENT,
	};

	MultiplayerState multiplayer_state = STATE_OFFLINE;
	void _update_state();
	void _peer_packet(int p_peer_id, const Vector<uint8_t> &p_packet);
	void _peer_authenticating(int peer_id);
	Error send_ping_to_server(int p_ping_request_index);

protected:
	static void _bind_methods();

public:
	enum CustomCommands {
		CUSTOM_COMMAND_PING = 1,
		CUSTOM_COMMAND_PONG = 2,
	};

	virtual void set_multiplayer_peer(const Ref<MultiplayerPeer> &p_peer) override;
	virtual Error poll() override;

	// check if its a server online (not a OfflineMultiplayerPeer)
	// usefull for rollback systems to avoid triggering rollback while offline
	virtual bool is_online_server() const;

	RollbackMultiplayer();
};
