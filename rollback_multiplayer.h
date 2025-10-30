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

	enum RollbackState {
		ROLLBACK_STATE_OFFLINE,
		ROLLBACK_STATE_SERVER,
		ROLLBACK_STATE_CLIENT,
	};

	RollbackState rollback_state = ROLLBACK_STATE_OFFLINE;
	RollbackState last_rollback_state = ROLLBACK_STATE_OFFLINE;

	PackedByteArray packet_cache;

	void _update_rollback_state();

	enum {
		CMD_FLAG_PING_PONG_SHIFT = 1 << SceneMultiplayer::CMD_FLAG_0_SHIFT,
	};

	enum {
		COMMAND_PING,
		COMMAND_PONG,
	};

	Error ping(); // ping the server
	void _process_ping(int p_peer_id, const uint8_t *p_packet, int p_packet_len);
	void _process_pong(int p_peer_id, const uint8_t *p_packet, int p_packet_len);

protected:
	static void _bind_methods();

	virtual void _process_packet(int p_from, const uint8_t *p_packet, int p_packet_len);

public:
	virtual void set_multiplayer_peer(const Ref<MultiplayerPeer> &p_peer) override;

	virtual Error poll() override;

	virtual Error object_configuration_add(Object *p_obj, Variant p_config) override;
	virtual Error object_configuration_remove(Object *p_obj, Variant p_config) override;

	// check if its a server online (not a OfflineMultiplayerPeer)
	// usefull for rollback systems to avoid triggering rollback while offline
	virtual bool is_online_server() const;

	RollbackMultiplayer();
};
