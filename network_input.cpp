#include "network_input.h"
#include "network.h"

PackedStringArray NetworkInput::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	NetworkInputReplicaConfig *r = replica_config.ptr();

	if (!r) {
		warnings.push_back(RTR("NetworkInput node has no replica configuration."));
		return warnings;
	}

	TypedArray<NodePath> properties = r->get_properties();
	for (const NodePath &property_name : properties) {
		bool prop_valid = false;
		const Variant &prop_value = get_indexed(property_name.get_names(), &prop_valid);
		if (!prop_valid) {
			warnings.push_back(RTR(vformat("NetworkInput property \"%s\" does not exist in object \"%s\".", property_name, this->get_name())));
			continue;
		}

		const Variant::Type prop_type = prop_value.get_type();

		// Only fixed length types are supported.
		switch (prop_type) {
			case Variant::BOOL:
			case Variant::INT:
			case Variant::FLOAT:
			case Variant::VECTOR2:
			case Variant::VECTOR2I:
			case Variant::RECT2:
			case Variant::RECT2I:
			case Variant::VECTOR3:
			case Variant::VECTOR3I:
			case Variant::TRANSFORM2D:
			case Variant::VECTOR4:
			case Variant::VECTOR4I:
			case Variant::PLANE:
			case Variant::QUATERNION:
			case Variant::AABB:
			case Variant::BASIS:
			case Variant::TRANSFORM3D:
			case Variant::PROJECTION:
				break;
			default:
				warnings.push_back(RTR(vformat("NetworkInput property \"%s\" has unsupported type \"%s\".", property_name, Variant::get_type_name(prop_type))));
				break;
		}
	}

	return warnings;
}

void NetworkInput::set_replica_config(Ref<NetworkInputReplicaConfig> p_config) {
	replica_config = p_config;
	update_configuration_warnings();
}

Ref<NetworkInputReplicaConfig> NetworkInput::get_replica_config() {
	return replica_config;
}

NetworkInputReplicaConfig *NetworkInput::get_replica_config_ptr() const {
	return replica_config.ptr();
}

void NetworkInput::buffer() {
	ERR_FAIL_COND_MSG(replica_config.is_null(), "Inputs buffering is not configured. Set a valid NetworkInputReplicaConfig resource to enable input buffering.");
	ERR_FAIL_COND(input_buffer.size() == 0);
	// let the dev handle this? is there even a case where we want to buffer during rollback?
	// ERR_FAIL_COND_MSG(Network::get_singleton()->is_in_rollback_frame(), "Cannot buffer inputs during a rollback frame.");

	GDVIRTUAL_CALL(_gather); // TODO: return if if the call have failed?

	const uint64_t tick = Network::get_singleton()->get_network_frames();

	// random insert the frame, dont care about its position in the buffer
	const int32_t idx = tick % input_buffer.size();

	// input_buffer[idx].properties.clear(); // if the config changes we should clear
	input_buffer[idx].tick = tick;

	TypedArray<NodePath> props = replica_config->get_properties();

	for (const NodePath &prop : props) {
		bool valid = false;
		const Variant &v = get_indexed(prop.get_names(), &valid);
		ERR_CONTINUE_MSG(!valid, vformat("Property '%s' not found.", prop));
		input_buffer[idx].properties.insert(prop, v);
	}
}

void NetworkInput::clear_buffer() {
	for (InputFrame &frame : input_buffer) {
		frame.tick = 0;
		frame.properties.clear();
	}
}

void NetworkInput::replay_input(uint64_t p_tick) {
	ERR_FAIL_COND_MSG(replica_config.is_null(), "Inputs buffering is not configured. Set a valid NetworkInputReplicaConfig resource to enable input buffering.");
	ERR_FAIL_COND(input_buffer.size() == 0);

	const int32_t idx = p_tick % input_buffer.size();
	const InputFrame &frame = input_buffer[idx];

	ERR_FAIL_COND_MSG(frame.tick != p_tick, vformat("No input buffered for tick %d.", p_tick));

	for (const KeyValue<NodePath, Variant> &kv : frame.properties) {
		set_indexed(kv.key.get_names(), kv.value);
	}
}

void NetworkInput::_bind_methods() {
	GDVIRTUAL_BIND(_gather);

	ClassDB::bind_method(D_METHOD("set_replica_config", "config"), &NetworkInput::set_replica_config);
	ClassDB::bind_method(D_METHOD("get_replica_config"), &NetworkInput::get_replica_config);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "replica_config", PROPERTY_HINT_RESOURCE_TYPE, "NetworkInputReplicaConfig", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_replica_config", "get_replica_config");
}

NetworkInput::NetworkInput() {
	input_buffer.resize(64);

	add_to_group(SNAME("_network_input"));
}
