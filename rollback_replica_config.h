#pragma once

#include "core/io/resource.h"
#include "core/variant/typed_array.h"

class NetworkReplicaConfig : public Resource {
	GDCLASS(NetworkReplicaConfig, Resource);
	OBJ_SAVE_TYPE(NetworkReplicaConfig);
	RES_BASE_EXTENSION("rrc");

protected:
	static void _bind_methods();
};
