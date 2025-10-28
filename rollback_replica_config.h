#pragma once

#include "core/io/resource.h"
#include "core/variant/typed_array.h"

// ReplicaPropertiesConfig
class RollbackReplicaConfig : public Resource {
	GDCLASS(RollbackReplicaConfig, Resource);
	OBJ_SAVE_TYPE(RollbackReplicaConfig);
	RES_BASE_EXTENSION("rrc");

protected:
	static void _bind_methods();
};
