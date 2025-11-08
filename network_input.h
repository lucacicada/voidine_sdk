#pragma once

#include "network_input_replica_config.h"

#include "core/object/ref_counted.h"
#include "scene/main/multiplayer_api.h"
#include "scene/main/node.h"

struct InputFrame {
	uint64_t frame_id = 0; // 0 means uninitialized
	HashMap<NodePath, Variant> properties;
};

class NetworkInput : public Node {
	GDCLASS(NetworkInput, Node)

private:
	uint64_t _last_frame_id = 0;
	RingBuffer<InputFrame *> input_buffer;
	Ref<NetworkInputReplicaConfig> replica_config;

	void _start();
	void _stop();

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void reset();

public:
	GDVIRTUAL0(_gather); // for compatibility

	void set_replica_config(Ref<NetworkInputReplicaConfig> p_config);
	Ref<NetworkInputReplicaConfig> get_replica_config();

	virtual void set_multiplayer_authority(int p_peer_id, bool p_recursive = true) override;
	PackedStringArray get_configuration_warnings() const override;

	void gather();
	void replay();
	void write_frame(InputFrame *p_frame);
	int copy_buffer(InputFrame **p_buf, int p_count) const;

	NetworkInput();
	~NetworkInput();
};
