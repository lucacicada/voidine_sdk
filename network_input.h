#pragma once

#include "network_input_replica_config.h"

#include "core/object/ref_counted.h"
#include "scene/main/multiplayer_api.h"
#include "scene/main/node.h"

// TODO: buffer should not track too much stuff
// it is the resposability of the replica interface to ensure data is sent/received consistently
// the ring buffer is also wrong as we might have predicted/simuated input that will be overwritten
// by the replica once authoritative data is received

class InputFrame {
public:
	uint64_t frame_id = 0; // 0 means uninitialized
	HashMap<NodePath, Variant> properties;

public:
	uint64_t get_frame_id() const {
		return frame_id;
	}
	void set_frame_id(uint64_t p_frame_id) {
		frame_id = p_frame_id;
	}

	HashMap<NodePath, Variant> get_data() const {
		return properties;
	}

	void set_property(const NodePath &p_property, const Variant &p_value) {
		properties.insert(p_property, p_value);
	}
};

class InputBuffer : public RefCounted {
	GDCLASS(InputBuffer, RefCounted);

private:
	uint64_t _last_frame_id = 0;
	uint32_t _head = 0;
	uint32_t _size = 0;
	uint32_t _read_head = 0;
	TightLocalVector<InputFrame> buffer;

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
		_last_frame_id = 0;
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

	void append(InputFrame &p_frame) {
		ERR_FAIL_COND_MSG(buffer.size() == 0, "Input buffer capacity is zero, cannot append frame.");
		ERR_FAIL_COND_MSG(p_frame.frame_id == 0, "Input frame tick cannot be zero.");
		ERR_FAIL_COND_MSG(p_frame.frame_id <= _last_frame_id, vformat("Cannot append tick %d, last is %d.", p_frame.frame_id, _last_frame_id));

		_last_frame_id = p_frame.frame_id;
		buffer[_head] = p_frame;

		const uint32_t capacity = buffer.size();
		_head = (_head + 1) % capacity;
		if (_size < capacity) {
			_size++;
		}
	}

	const InputFrame *get_frame(uint64_t p_tick) const {
		const uint32_t capacity = buffer.size();
		for (uint32_t i = 0; i < _size; i++) {
			const InputFrame &frame = buffer[(_head + capacity - _size + i) % capacity];
			if (frame.frame_id == p_tick) {
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
		return buffer[(_head + capacity - _size + p_index) % capacity].frame_id;
	}
	HashMap<NodePath, Variant> get_frame_data(uint32_t p_index) const {
		if (p_index >= _size) {
			return HashMap<NodePath, Variant>();
		}
		const uint32_t capacity = buffer.size();
		return buffer[(_head + capacity - _size + p_index) % capacity].properties;
	}

	uint64_t next_frame_id() const {
		return _last_frame_id + 1;
	}
	uint64_t get_last_frame_id() const {
		return _last_frame_id;
	}

	void set_read_head(uint32_t idx) {
		_read_head = idx % buffer.size();
	}
	uint32_t get_read_head() const {
		return _read_head;
	}
	void advance_read_head() {
		if (_size == 0) {
			return;
		}
		_read_head = (_read_head + 1) % buffer.size();
	}
	const InputFrame &get_read_frame() const {
		return buffer[_read_head];
	}
	void reset_read_head_to_oldest() {
		const uint32_t capacity = buffer.size();
		_read_head = (_head + capacity - _size) % capacity;
	}

	uint32_t unread_count() const {
		if (_size == 0) {
			return 0;
		}
		const uint32_t capacity = buffer.size();
		uint32_t newest_idx = (_head + capacity - 1) % capacity;
		if (_read_head > newest_idx) {
			return (capacity - _read_head) + (newest_idx + 1);
		} else {
			return (newest_idx + 1) - _read_head;
		}
	}
};

class NetworkInput : public Node {
	GDCLASS(NetworkInput, Node)

private:
	RingBuffer<InputFrame> input_buffer;

	Ref<InputBuffer> buffer;
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
	void get_last_input_frames_asc(uint32_t p_count, Vector<const InputFrame *> &r_frames) const;
	// void replay(uint64_t p_frame_id);

	Ref<InputBuffer> get_input_buffer() const { return buffer; }

	// RingBuffer<InputFrame> &get_raw_input_buffer() { return input_buffer; }
	int copy_input_buffer(InputFrame *p_buf, int p_count) const;

	NetworkInput();
	~NetworkInput();
};
