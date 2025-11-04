#include "network_input.h"
#include "network.h"

PackedStringArray NetworkInput::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (replica_config.is_null()) {
		warnings.push_back(RTR("NetworkInput node has no replica configuration."));
		return warnings;
	}

	const TypedArray<NodePath> props = replica_config->get_properties();

	for (const NodePath &property_name : props) {
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

void NetworkInput::set_multiplayer_authority(int p_peer_id, bool p_recursive) {
	if (get_multiplayer_authority() == p_peer_id) {
		return;
	}

	_stop();
	Node::set_multiplayer_authority(p_peer_id, p_recursive);
	_start();
}

void NetworkInput::gather() {
	ERR_FAIL_COND_MSG(replica_config.is_null(), "Inputs buffering is not configured. Set a valid NetworkInputReplicaConfig resource to enable input buffering.");

	// let the dev handle this? is there even a case where we want to buffer during rollback?
	// ERR_FAIL_COND_MSG(Network::get_singleton()->is_in_rollback_frame(), "Cannot buffer inputs during a rollback frame.");

	const uint64_t tick = Network::get_singleton()->get_network_frames();
	ERR_FAIL_COND_MSG(tick <= last_buffered_tick, vformat("Cannot buffer old tick %d, last is %d.", tick, last_buffered_tick));
	last_buffered_tick = tick;

	GDVIRTUAL_CALL(_gather); // TODO: return if if the call have failed?

	input_buffer.write[input_buffer_head].tick = tick;
	// input_buffer[input_buffer_head].properties.clear(); // TODO: if the config changes we should clear

	const TypedArray<NodePath> props = replica_config->get_properties();

	for (const NodePath &prop : props) {
		bool valid = false;
		const Variant &v = get_indexed(prop.get_names(), &valid);
		ERR_CONTINUE_MSG(!valid, vformat("Property '%s' not found.", prop));
		input_buffer.write[input_buffer_head].properties.insert(prop, v);
	}

	input_buffer_head = (input_buffer_head + 1) % input_buffer.size();
	if (input_buffer_size < input_buffer.size()) {
		input_buffer_size++;
	}

	if (input_buffer_size == input_buffer.size()) {
		earliest_buffered_tick = input_buffer[input_buffer_head].tick;
	} else {
		earliest_buffered_tick = input_buffer[0].tick;
	}
}

void NetworkInput::get_last_input_frames_asc(uint32_t p_count, Vector<const InputFrame *> &r_frames) const {
	ERR_FAIL_COND_MSG(replica_config.is_null(), "Inputs buffering is not configured. Set a valid NetworkInputReplicaConfig resource to enable input buffering.");

	p_count = MIN(p_count, input_buffer_size);

	if (p_count == 0) {
		return;
	}

	r_frames.resize(p_count);

	const uint32_t capacity = input_buffer.size();

	for (uint32_t i = 0; i < p_count; i++) {
		const int32_t idx = (input_buffer_head + capacity - p_count + i) % capacity;
		r_frames.write[i] = &input_buffer[idx];
	}
}

void NetworkInput::push_frame(InputFrame p_frame) {
	ERR_FAIL_COND(p_frame.tick <= last_buffered_tick);
	last_buffered_tick = p_frame.tick;

	// append to the input buffer
	input_buffer.write[input_buffer_head] = p_frame;
	input_buffer_head = (input_buffer_head + 1) % input_buffer.size();
	if (input_buffer_size < input_buffer.size()) {
		input_buffer_size++;
	}

	const TypedArray<NodePath> props = replica_config->get_properties();

	for (const NodePath &prop : props) {
		ERR_CONTINUE_MSG(!p_frame.properties.has(prop), vformat("Remote input frame is missing property '%s'.", prop));
		set_indexed(prop.get_names(), p_frame.properties.get(prop));
	}
}

void NetworkInput::_start() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	get_multiplayer()->object_configuration_add(this, Variant());
}

void NetworkInput::_stop() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	get_multiplayer()->object_configuration_remove(this, Variant());
}

void NetworkInput::_notification(int p_what) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_start();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_stop();
		} break;
	}
}

void NetworkInput::_bind_methods() {
	GDVIRTUAL_BIND(_gather);

	ClassDB::bind_method(D_METHOD("set_replica_config", "config"), &NetworkInput::set_replica_config);
	ClassDB::bind_method(D_METHOD("get_replica_config"), &NetworkInput::get_replica_config);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "replica_config", PROPERTY_HINT_RESOURCE_TYPE, "NetworkInputReplicaConfig", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_replica_config", "get_replica_config");

	// signal when the input is restored, give the dev a change to normalize the input
	ADD_SIGNAL(MethodInfo("replayed"));
}

NetworkInput::NetworkInput() {
	input_buffer.reserve_exact(64);
	input_buffer.resize(64);
}

NetworkInput::~NetworkInput() {
}
