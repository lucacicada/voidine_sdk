#pragma once

#include "network_input_replica_config.h"

#include "core/object/ref_counted.h"
#include "scene/main/multiplayer_api.h"
#include "scene/main/node.h"

// class InputBuffer : public RefCounted {
// 	GDCLASS(InputBuffer, RefCounted);

// public:
// 	struct Buffer_InputFrame {
// 		uint64_t tick = 0;
// 		HashMap<NodePath, Variant> properties;
// 	};

// private:
// 	uint64_t earliest_buffered_tick = 0;
// 	uint64_t last_buffered_tick = 0;

// 	uint32_t _head = 0;
// 	uint32_t _size = 0;
// 	TightLocalVector<Buffer_InputFrame> buffer;

// public:
// 	uint32_t capacity() const { return buffer.size(); }
// 	uint32_t size() const { return _size; }
// 	bool is_empty() const { return _size == 0; }

// 	// buffer[0] -> oldest frame
// 	// buffer[size() - 1] -> newest frame
// 	Buffer_InputFrame &operator[](uint32_t idx) {
// 		const uint32_t capacity = buffer.size();
// 		uint32_t real_idx = (_head + capacity - _size + idx) % capacity;
// 		return buffer[real_idx];
// 	}

// 	const Buffer_InputFrame &operator[](uint32_t idx) const {
// 		const uint32_t capacity = buffer.size();
// 		uint32_t real_idx = (_head + capacity - _size + idx) % capacity;
// 		return buffer[real_idx];
// 	}

// 	bool has_frame(uint64_t p_tick) const {
// 		return get_frame(p_tick) != nullptr;
// 	}

// 	void clear() {
// 		_head = 0;
// 		_size = 0;
// 	}

// 	void resize(uint32_t p_capacity) {
// 		ERR_FAIL_COND_MSG(p_capacity == 0, "Input buffer capacity must be greater than zero.");

// 		if (p_capacity == buffer.size()) {
// 			return;
// 		}

// 		earliest_buffered_tick = 0;
// 		last_buffered_tick = 0;
// 		_head = 0;
// 		_size = 0;
// 		buffer.resize(p_capacity);
// 	}

// 	void append(const Buffer_InputFrame &p_frame) {
// 		ERR_FAIL_COND(p_frame.tick <= last_buffered_tick);
// 		last_buffered_tick = p_frame.tick;

// 		const uint32_t capacity = buffer.size();

// 		buffer[_head].tick = p_frame.tick;
// 		// buffer[_head].properties.clear(); // unsafe, but faster
// 		buffer[_head].properties = p_frame.properties;

// 		_head = (_head + 1) % capacity;
// 		if (_size < capacity) {
// 			_size++;
// 		}

// 		if (_size == capacity) {
// 			uint32_t earliest_idx = _head;
// 			earliest_buffered_tick = buffer[earliest_idx].tick;
// 		} else {
// 			earliest_buffered_tick = buffer[0].tick;
// 		}
// 	}

// 	const Buffer_InputFrame *get_frame(uint64_t p_tick) const {
// 		const uint32_t capacity = buffer.size();
// 		for (uint32_t i = 0; i < _size; i++) {
// 			const Buffer_InputFrame &frame = buffer[(_head + capacity - _size + i) % capacity];
// 			if (frame.tick == p_tick) {
// 				return &frame;
// 			}
// 		}

// 		return nullptr;
// 	}

// 	InputBuffer() {
// 		resize(1); // ensure the buffer is never empty
// 	}
// };

class NetworkInput : public Node {
	GDCLASS(NetworkInput, Node)

public:
	struct InputFrame {
		uint64_t tick = 0;
		HashMap<NodePath, Variant> properties;
	};

private:
	uint32_t input_buffer_head = 0;
	uint32_t input_buffer_size = 0;
	uint64_t earliest_buffered_tick = 0;
	uint64_t last_buffered_tick = 0;
	Vector<InputFrame> input_buffer;

private:
	Ref<NetworkInputReplicaConfig> replica_config;

	void _start();
	void _stop();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	GDVIRTUAL0(_gather); // for compatibility

	void set_replica_config(Ref<NetworkInputReplicaConfig> p_config);
	Ref<NetworkInputReplicaConfig> get_replica_config();

	virtual void set_multiplayer_authority(int p_peer_id, bool p_recursive = true) override;
	PackedStringArray get_configuration_warnings() const override;

	void gather();
	void push_frame(InputFrame p_frame);
	// void replay_input(uint64_t p_tick);
	// bool has_frame(uint64_t p_tick) const;
	void get_last_input_frames_asc(uint32_t p_count, Vector<const InputFrame *> &r_frames) const;

	NetworkInput();
	~NetworkInput();
};
