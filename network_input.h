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
	struct InputSample {
		uint32_t samples = 0;
		Variant accumulated;
	};

	// TODO: Use StringName instead of NodePath
	HashMap<NodePath, InputSample> _samples;

	uint64_t _last_frame_id = 0;
	RingBuffer<InputFrame> buffer;

	Ref<NetworkInputReplicaConfig> replica_config;

	void _start();
	void _stop();

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void reset();

public:
	GDVIRTUAL0(_gather); // for compatibility

	virtual void set_multiplayer_authority(int p_peer_id, bool p_recursive = true) override;

	void set_replica_config(Ref<NetworkInputReplicaConfig> p_config);
	Ref<NetworkInputReplicaConfig> get_replica_config();

	void sample(const NodePath &p_property, const Variant &p_value);

	void gather();
	void replay();

	Error copy_buffer(Vector<InputFrame> &frames, int p_count);
	void write_frame(const InputFrame &p_frame);

	bool is_input_authority() const;

	NetworkInput();
};
