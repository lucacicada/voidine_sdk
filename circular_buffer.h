#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"
#include "core/variant/variant.h"

class CircularBuffer : public RefCounted {
	GDCLASS(CircularBuffer, RefCounted)

private:
	LocalVector<Variant> data_array;
	int _head;
	int _size;

protected:
	static void _bind_methods();

public:
	bool is_empty() const;
	int capacity() const;
	int size() const;
	int head() const;

	void clear();
	void resize(const int p_capacity);
	void fill(const Variant &p_value);

	void advance(const int p_steps = 1);
	void seek(const int p_pos);
	void set_size(const int p_size);

	void append(const Variant &p_value);
	void append_array(const Array &p_array);
	void insert(int p_pos, const Variant &p_value);

	void push_front(const Variant &p_value);
	void push_back(const Variant &p_value);

	Variant pop_back();
	Variant pop_front();
	Variant pop_at(int p_pos);

	Variant front() const;
	Variant back() const;

	Variant at(const int p_index) const;
	Variant array_get(const int p_index) const;
	void array_set(const int p_index, const Variant &p_value);

	Array duplicate() const;

	Array slice(int p_begin, int p_end = INT_MAX, int p_step = 1) const;

	CircularBuffer() = default;
};
