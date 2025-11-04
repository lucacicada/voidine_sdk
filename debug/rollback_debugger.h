
#pragma once

#include "core/debugger/engine_profiler.h"

class RollbackDebugger {
private:
	class InputProfiler : public EngineProfiler {
		GDSOFTCLASS(InputProfiler, EngineProfiler);
	};

private:
	static Error _capture(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured);

public:
	static void initialize();
	static void deinitialize();
};
