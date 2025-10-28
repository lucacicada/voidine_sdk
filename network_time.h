#pragma once

#include "core/object/class_db.h"
#include "core/os/os.h"

class NetworkTime : public Object {
	GDCLASS(NetworkTime, Object);

private:
	inline static NetworkTime *singleton = nullptr;

private:
	int64_t offset = 0;

protected:
	static void _bind_methods();

public:
	static NetworkTime *get_singleton();

	double get_time_seconds() const;
	uint64_t get_ticks_msec() const;
	uint64_t get_ticks_usec() const;

	void set_time(int64_t p_time);
	void set_offset(int64_t p_offset);
	void adjust_time(int64_t p_offset);

	NetworkTime();
	virtual ~NetworkTime();
};
