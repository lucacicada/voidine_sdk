#pragma once

#include "core/io/resource.h"
#include "core/variant/typed_array.h"

class NetworkActorReplicaConfig : public Resource {
	GDCLASS(NetworkActorReplicaConfig, Resource);
	OBJ_SAVE_TYPE(NetworkActorReplicaConfig);
	RES_BASE_EXTENSION("rrc");

protected:
	static void _bind_methods();
};
