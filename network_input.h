#pragma once

#include "network_input_replica_config.h"

#include "core/object/ref_counted.h"
#include "scene/main/multiplayer_api.h"
#include "scene/main/node.h"

// TODO: buffer should not track too much stuff
// it is the resposability of the replica interface to ensure data is sent/received consistently
// the ring buffer is also wrong as we might have predicted/simuated input that will be overwritten
// by the replica once authoritative data is received

struct InputFrame {
	enum {
		FLAG_SIMULATED = 1 << 0,
		FLAG_PREDICTED = 1 << 1,
	};

	uint32_t flags = 0;
	uint64_t tick = 0; // 0 indicate this frame is uninitialized
	HashMap<NodePath, Variant> properties;

	bool is_simulated() {
		return flags & FLAG_SIMULATED;
	}
	bool is_predicted() {
		return flags & FLAG_PREDICTED;
	}

	uint64_t get_tick() const {
		return tick;
	}
	HashMap<NodePath, Variant> get_data() const {
		return properties;
	}
};

class InputBuffer : public RefCounted {
	GDCLASS(InputBuffer, RefCounted);

private:
	uint64_t _last_tick = 0;
	uint32_t _head = 0;
	uint32_t _size = 0;
	TightLocalVector<InputFrame> buffer;

protected:
	static void _bind_methods();

public:
	struct Iterator {
		InputBuffer *buffer;
		uint32_t idx;

		_FORCE_INLINE_ InputFrame &operator*() { return (*buffer)[idx]; }
		_FORCE_INLINE_ InputFrame *operator->() const { return &(*buffer)[idx]; }

		_FORCE_INLINE_ Iterator &operator++() {
			idx++;
			return *this;
		}
		_FORCE_INLINE_ Iterator &operator--() {
			idx--;
			return *this;
		}

		_FORCE_INLINE_ bool operator==(const Iterator &other) const { return idx == other.idx; }
		_FORCE_INLINE_ bool operator!=(const Iterator &other) const { return idx != other.idx; }

		Iterator(InputBuffer *buf, uint32_t i) : buffer(buf), idx(i) {}
	};

	// buffer[0] -> oldest frame
	// buffer[size() - 1] -> newest frame
	InputFrame &operator[](uint32_t idx) {
		CRASH_BAD_UNSIGNED_INDEX(idx, _size);
		const uint32_t capacity = buffer.size();
		uint32_t real_idx = (_head + capacity - _size + idx) % capacity;
		return buffer[real_idx];
	}

	const InputFrame &operator[](uint32_t idx) const {
		CRASH_BAD_UNSIGNED_INDEX(idx, _size);
		const uint32_t capacity = buffer.size();
		uint32_t real_idx = (_head + capacity - _size + idx) % capacity;
		return buffer[real_idx];
	}

	uint32_t capacity() const { return buffer.size(); }
	uint32_t size() const { return _size; }
	bool is_empty() const { return _size == 0; }

	bool has_frame(uint64_t p_tick) const {
		return get_frame(p_tick) != nullptr;
	}

	void clear() {
		_head = 0;
		_size = 0;
		for (uint32_t i = 0; i < buffer.size(); i++) {
			buffer[i] = InputFrame();
		}
	}

	void resize(uint32_t p_capacity) {
		ERR_FAIL_COND_MSG(p_capacity == 0, "Input buffer capacity must be greater than zero.");

		if (p_capacity == buffer.size()) {
			return;
		}

		buffer.resize(p_capacity);
		clear(); // clear when resizing
	}

	void append(const InputFrame &p_frame) {
		ERR_FAIL_COND_MSG(p_frame.tick == 0, "Input frame tick cannot be zero.");
		ERR_FAIL_COND_MSG(buffer.size() == 0, "Input buffer capacity is zero, cannot append frame.");
		ERR_FAIL_COND_MSG(p_frame.tick <= _last_tick, vformat("Cannot append tick %d, last is %d.", p_frame.tick, _last_tick));
		_last_tick = p_frame.tick;

		const uint32_t capacity = buffer.size();
		buffer[_head] = p_frame;
		_head = (_head + 1) % capacity;
		if (_size < capacity) {
			_size++;
		}
	}

	const InputFrame *get_frame(uint64_t p_tick) const {
		const uint32_t capacity = buffer.size();
		for (uint32_t i = 0; i < _size; i++) {
			const InputFrame &frame = buffer[(_head + capacity - _size + i) % capacity];
			if (frame.tick == p_tick) {
				return &frame;
			}
		}

		return nullptr;
	}

	InputFrame front() const { // return the oldest frame
		ERR_FAIL_COND_V(_size == 0, InputFrame());
		const uint32_t capacity = buffer.size();
		return buffer[(_head + capacity - _size) % capacity];
	}
	InputFrame back() const { // return the most recent frame
		ERR_FAIL_COND_V(_size == 0, InputFrame());
		const uint32_t capacity = buffer.size();
		return buffer[(_head + capacity - 1) % capacity];
	}

	uint64_t get_frame_tick(uint32_t p_index) const {
		if (p_index >= _size) {
			return 0;
		}
		const uint32_t capacity = buffer.size();
		return buffer[(_head + capacity - _size + p_index) % capacity].tick;
	}
	HashMap<NodePath, Variant> get_frame_data(uint32_t p_index) const {
		if (p_index >= _size) {
			return HashMap<NodePath, Variant>();
		}
		const uint32_t capacity = buffer.size();
		return buffer[(_head + capacity - _size + p_index) % capacity].properties;
	}
};

class NetworkInput : public Node {
	GDCLASS(NetworkInput, Node)

private:
	uint32_t input_buffer_head = 0;
	uint32_t input_buffer_size = 0;
	uint64_t earliest_buffered_tick = 0;
	uint64_t last_buffered_tick = 0;
	Vector<InputFrame> input_buffer;

private:
	Ref<InputBuffer> buffer;
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

	int get_buffer_size() { return input_buffer_size; }
	int get_buffer_frame_tick(int p_index);
	Variant get_buffer_frame_value(int p_index, const NodePath &p_property); // get_frame_data
	int get_earliest_buffered_tick() { return earliest_buffered_tick; }
	int get_last_buffered_tick() { return last_buffered_tick; }
	void buffer_clear();

	NetworkInput();
	~NetworkInput();
};
