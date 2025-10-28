#include "circular_buffer.h"

bool CircularBuffer::is_empty() const {
	return _size == 0;
}

int CircularBuffer::capacity() const {
	return data_array.size();
}

int CircularBuffer::size() const {
	return _size;
}

int CircularBuffer::head() const {
	return _head;
}

void CircularBuffer::clear() {
	// Do not actually clear anything, just reset head and size
	_head = 0;
	_size = 0;
}

void CircularBuffer::resize(const int p_capacity) {
	ERR_FAIL_COND(p_capacity < 0);

	_head = 0;
	_size = 0;
	data_array.resize(p_capacity);
	for (int i = 0; i < p_capacity; i++) {
		data_array[i] = Variant();
	}
}

void CircularBuffer::fill(const Variant &p_value) {
	const int s = data_array.size();
	if (s == 0) {
		return;
	}

	_head = 0;
	_size = s;
	for (int i = 0; i < s; i++) {
		data_array[i] = p_value;
	}
}

void CircularBuffer::advance(int p_steps) {
	const int s = data_array.size();
	if (s == 0) {
		return;
	}

	// Allow arbitrary large positive or negative values
	_head = (_head + (p_steps % s) + s) % s;
}

void CircularBuffer::seek(const int p_pos) {
	ERR_FAIL_COND(p_pos < 0 || p_pos > (int)data_array.size());
	_head = p_pos;
}

void CircularBuffer::set_size(const int p_size) {
	ERR_FAIL_COND(p_size < 0 || p_size > (int)data_array.size());
	_size = p_size;
}

void CircularBuffer::append(const Variant &p_value) {
	const int s = data_array.size();
	if (s == 0) {
		return;
	}

	data_array[_head] = p_value;

	_head = (_head + 1) % s;
	if (_size < s) {
		_size++;
	}
}

void CircularBuffer::append_array(const Array &p_array) {
	const int s = data_array.size();
	if (s == 0) {
		return;
	}

	const int p_size = p_array.size();
	for (int i = 0; i < p_size; i++) {
		data_array[_head] = p_array[i];
		_head = (_head + 1) % s;
		if (_size < s) {
			_size++;
		}
	}
}

Variant CircularBuffer::front() const {
	ERR_FAIL_COND_V_MSG(_size == 0, Variant(), "Can't take value from empty buffer.");

	const int s = data_array.size();
	const int index = (_head - 1 + s) % s;
	return data_array[index];
}

Variant CircularBuffer::back() const {
	ERR_FAIL_COND_V_MSG(_size == 0, Variant(), "Can't take value from empty buffer.");

	const int s = data_array.size();
	const int index = (_head - _size + s) % s;
	return data_array[index];
}

Variant CircularBuffer::at(int p_index) const {
	const int s = data_array.size();

	// Index must be within bounds, from -size to size-1
	ERR_FAIL_COND_V_MSG(p_index < -_size || p_index >= _size, Variant(), "Index " + itos(p_index) + " is out of bounds (size: " + itos(_size) + ").");

	if (p_index < 0) {
		p_index += _size;
	}

	p_index = (s + _head - _size + p_index) % s;

	return data_array[p_index];
}

Variant CircularBuffer::array_get(int p_index) const {
	const int s = data_array.size();

	if (_size == 0 || s == 0 || p_index < 0 || p_index >= _size) {
		return Variant();
	}

	p_index = (s + _head - _size + p_index) % s;

	return data_array[p_index];
}

void CircularBuffer::array_set(int p_index, const Variant &p_value) {
	const int s = data_array.size();
	if (_size == 0 || s == 0 || p_index < 0 || p_index >= _size) {
		return;
	}

	p_index = (s + _head - _size + p_index) % s;

	data_array[p_index] = p_value;
}

Array CircularBuffer::slice(int p_begin, int p_end, int p_step) const {
	Array result;

	ERR_FAIL_COND_V_MSG(p_step == 0, result, "Slice step cannot be zero.");

	const int s = _size;

	if (s == 0 || (p_begin < -s && p_step < 0) || (p_begin >= s && p_step > 0)) {
		return result;
	}

	int begin = CLAMP(p_begin, -s, s - 1);
	if (begin < 0) {
		begin += s;
	}
	int end = CLAMP(p_end, -s - 1, s);
	if (end < 0) {
		end += s;
	}

	ERR_FAIL_COND_V_MSG(p_step > 0 && begin > end, result, "Slice step is positive, but bounds are decreasing.");
	ERR_FAIL_COND_V_MSG(p_step < 0 && begin < end, result, "Slice step is negative, but bounds are increasing.");

	int result_size = (end - begin) / p_step + (((end - begin) % p_step != 0) ? 1 : 0);
	result.resize(result_size);

	for (int src_idx = begin, dest_idx = 0; dest_idx < result_size; ++dest_idx) {
		result[dest_idx] = at(src_idx);
		src_idx += p_step;
	}

	return result;
}

void CircularBuffer::push_front(const Variant &p_value) {
	const int s = data_array.size();
	if (s == 0) {
		return;
	}

	// Move head backwards and insert
	_head = (_head - 1 + s) % s;
	data_array[_head] = p_value;

	if (_size < s) {
		_size++;
	}
}

void CircularBuffer::push_back(const Variant &p_value) {
	// push_back is the same as append
	append(p_value);
}

Variant CircularBuffer::pop_back() {
	ERR_FAIL_COND_V_MSG(_size == 0, Variant(), "Can't pop from empty buffer.");

	const int s = data_array.size();
	// Get the last element (most recently added)
	int index = (_head - 1 + s) % s;
	Variant result = data_array[index];

	// Move head backwards
	_head = index;
	_size--;

	return result;
}

Variant CircularBuffer::pop_front() {
	ERR_FAIL_COND_V_MSG(_size == 0, Variant(), "Can't pop from empty buffer.");

	const int c = data_array.size();
	// Get the first element (oldest)
	int index = (_head - _size + c) % c;
	Variant result = data_array[index];

	// Just decrease size, don't move head
	_size--;

	return result;
}

Variant CircularBuffer::pop_at(int p_pos) {
	ERR_FAIL_COND_V_MSG(_size == 0, Variant(), "Can't pop from empty buffer.");
	ERR_FAIL_COND_V_MSG(p_pos < 0 || p_pos >= _size, Variant(), "Index out of bounds.");

	const int c = data_array.size();
	int actual_index = (c + _head - _size + p_pos) % c;
	Variant result = data_array[actual_index];

	// Shift elements to fill the gap
	for (int i = p_pos; i < _size - 1; i++) {
		int current_index = (c + _head - _size + i) % c;
		int next_index = (c + _head - _size + i + 1) % c;
		data_array[current_index] = data_array[next_index];
	}

	_size--;
	return result;
}

void CircularBuffer::insert(int p_pos, const Variant &p_value) {
	const int c = data_array.size();
	if (c == 0) {
		return;
	}

	ERR_FAIL_COND_MSG(p_pos < 0 || p_pos > _size, "Insert position out of bounds.");

	if (_size >= c) {
		// Buffer is full, can't insert
		return;
	}

	// Shift elements to make space
	for (int i = _size; i > p_pos; i--) {
		int current_index = (c + _head - _size + i - 1) % c;
		int next_index = (c + _head - _size + i) % c;
		data_array[next_index] = data_array[current_index];
	}

	// Insert the new value
	int insert_index = (c + _head - _size + p_pos) % c;
	data_array[insert_index] = p_value;
	_size++;
}

Array CircularBuffer::duplicate() const {
	Array result;
	result.resize(_size);

	for (int i = 0; i < _size; i++) {
		result[i] = at(i);
	}

	return result;
}

void CircularBuffer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_empty"), &CircularBuffer::is_empty);
	ClassDB::bind_method(D_METHOD("capacity"), &CircularBuffer::capacity);
	ClassDB::bind_method(D_METHOD("size"), &CircularBuffer::size);
	ClassDB::bind_method(D_METHOD("head"), &CircularBuffer::head);

	ClassDB::bind_method(D_METHOD("clear"), &CircularBuffer::clear);
	ClassDB::bind_method(D_METHOD("resize", "capacity"), &CircularBuffer::resize);
	ClassDB::bind_method(D_METHOD("fill", "value"), &CircularBuffer::fill);

	ClassDB::bind_method(D_METHOD("advance", "steps"), &CircularBuffer::advance, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("seek", "position"), &CircularBuffer::seek);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &CircularBuffer::set_size);

	ClassDB::bind_method(D_METHOD("append", "value"), &CircularBuffer::append);
	ClassDB::bind_method(D_METHOD("append_array", "array"), &CircularBuffer::append_array);
	ClassDB::bind_method(D_METHOD("insert", "position", "value"), &CircularBuffer::insert);

	ClassDB::bind_method(D_METHOD("push_front", "value"), &CircularBuffer::push_front);
	ClassDB::bind_method(D_METHOD("push_back", "value"), &CircularBuffer::push_back);

	ClassDB::bind_method(D_METHOD("pop_back"), &CircularBuffer::pop_back);
	ClassDB::bind_method(D_METHOD("pop_front"), &CircularBuffer::pop_front);
	ClassDB::bind_method(D_METHOD("pop_at", "position"), &CircularBuffer::pop_at);

	ClassDB::bind_method(D_METHOD("front"), &CircularBuffer::front);
	ClassDB::bind_method(D_METHOD("back"), &CircularBuffer::back);

	ClassDB::bind_method(D_METHOD("at", "index"), &CircularBuffer::at);
	ClassDB::bind_method(D_METHOD("array_get", "index"), &CircularBuffer::array_get);
	ClassDB::bind_method(D_METHOD("array_set", "index", "value"), &CircularBuffer::array_set);

	ClassDB::bind_method(D_METHOD("duplicate"), &CircularBuffer::duplicate);

	ClassDB::bind_method(D_METHOD("slice", "begin", "end", "step"), &CircularBuffer::slice, DEFVAL(INT_MAX), DEFVAL(1));
}
