#pragma once

#include "core/os/os.h"
#include "core/templates/local_vector.h"

/**
 * A simple rescheduling timer in microseconds.
 *
 * A rescheduling timer automatically adjusts its next trigger time based on
 * it's wait time, ensuring consistent intervals even if checks
 * are delayed.
 */
struct MicroTimer {
	double wait_time = 1.0; // The wait time, in seconds.
	uint64_t ticks = 0; // The last elapsed time in microseconds.

	/**
	 * Starts or restarts the timer.
	 */
	void start() {
		ticks = OS::get_singleton()->get_ticks_usec();
	}

	/**
	 * Checks if the timer have elapsed, and advances it if so.
	 */
	bool elapsed() {
		constexpr uint64_t USEC_PER_SEC = 1000000ULL;
		const uint64_t now = OS::get_singleton()->get_ticks_usec();
		const uint64_t interval = (uint64_t)(wait_time * USEC_PER_SEC);
		if (now - ticks >= interval) {
			ticks += ((now - ticks) / interval) * interval;
			return true;
		}
		return false;
	}
};

/**
 * A compact fixed size buffer that overwrites old data when full.
 */
template <typename T>
class FixedBuffer {
private:
	TightLocalVector<T> data;
	uint32_t head = 0;
	uint32_t count = 0;

public:
	_FORCE_INLINE_ const T &operator[](uint32_t p_index) const {
		ERR_FAIL_INDEX_V(p_index, count, T());
		return data[p_index % data.size()];
	}
	_FORCE_INLINE_ T &operator[](uint32_t p_index) {
		ERR_FAIL_INDEX_V(p_index, count, T());
		return data[p_index % data.size()];
	}

	_FORCE_INLINE_ uint32_t size() const { return count; }
	_FORCE_INLINE_ uint32_t capacity() const { return data.size(); }
	_FORCE_INLINE_ uint32_t space_left() const { return data.size() - count; }

	_FORCE_INLINE_ void clear() {
		head = 0;
		count = 0;
	}

	void append(const T &value) {
		const uint32_t cap = capacity();
		ERR_FAIL_COND(cap == 0);
		data[head] = value;
		head = (head + 1) % cap;
		if (count < cap) {
			count++;
		}
	}

	void resize(uint32_t p_capacity) {
		data.resize(p_capacity);
		head = 0;
		count = 0;
	}

	template <typename Comparator, bool Validate = SORT_ARRAY_VALIDATE_ENABLED, typename... Args>
	Vector<T> sort_custom(Args &&...args) {
		Vector<T> ret;
		ret.resize(count);
		memcpy(ret.ptrw(), data.ptr(), sizeof(T) * count);
		ret.sort_custom<Comparator, Validate>(std::forward<Args>(args)...);
		return ret;
	}

	// FixedBuffer() {}
	FixedBuffer(uint32_t p_capacity) { data.resize(p_capacity); }
};
