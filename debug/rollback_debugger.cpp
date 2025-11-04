#include "rollback_debugger.h"

#include "core/debugger/engine_debugger.h"
#include "core/os/os.h"
#include "scene/main/node.h"

void RollbackDebugger::initialize() {
	EngineDebugger::register_message_capture("rollback", EngineDebugger::Capture(nullptr, &_capture));
}

void RollbackDebugger::deinitialize() {
}

Error RollbackDebugger::_capture(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured) {
	return OK;
}
