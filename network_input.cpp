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

	buffer->clear();
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

	GDVIRTUAL_CALL(_gather); // TODO: return if if the call have failed?

	const TypedArray<NodePath> props = replica_config->get_properties();

	InputFrame frame;
	frame.frame_id = buffer->next_frame_id();

	for (const NodePath &prop : props) {
		bool valid = false;
		const Variant &v = get_indexed(prop.get_names(), &valid);
		ERR_CONTINUE_MSG(!valid, vformat("Property '%s' not found.", prop));
		frame.set_property(prop, v);
	}

	buffer->append(frame);

	if (input_buffer.space_left() == 0) {
		input_buffer.advance_read(1); // buffer full, advance read pointer to overwrite oldest
	}
	input_buffer.write(frame);
}

void NetworkInput::get_last_input_frames_asc(uint32_t p_count, Vector<const InputFrame *> &r_frames) const {
	ERR_FAIL_COND_MSG(replica_config.is_null(), "Inputs buffering is not configured. Set a valid NetworkInputReplicaConfig resource to enable input buffering.");

	p_count = MIN(p_count, buffer->size());

	if (p_count == 0) {
		return;
	}

	r_frames.resize(p_count);

	for (uint32_t i = 0; i < p_count; i++) {
		r_frames.write[i] = &buffer->operator[](buffer->size() - p_count + i);
	}
}

int NetworkInput::copy_input_buffer(InputFrame *p_buf, int p_count) const {
	p_count = MIN(p_count, input_buffer.data_left());
	return input_buffer.copy(p_buf, input_buffer.data_left() - p_count, p_count);
}

void NetworkInput::_bind_methods() {
	GDVIRTUAL_BIND(_gather);

	ClassDB::bind_method(D_METHOD("set_replica_config", "config"), &NetworkInput::set_replica_config);
	ClassDB::bind_method(D_METHOD("get_replica_config"), &NetworkInput::get_replica_config);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "replica_config", PROPERTY_HINT_RESOURCE_TYPE, "NetworkInputReplicaConfig", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_replica_config", "get_replica_config");
}

NetworkInput::NetworkInput() {
	buffer.instantiate();
	buffer->resize(64); // 1+ sec of buffer time

	input_buffer.resize(6); // 2^6 = 64 frames
}

NetworkInput::~NetworkInput() {
	buffer.unref();
}

void NetworkInput::reset() {
	buffer->clear();
	input_buffer.clear();
}

void NetworkInput::_start() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	reset();
	get_multiplayer()->object_configuration_add(this, Variant());
}

void NetworkInput::_stop() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	get_multiplayer()->object_configuration_remove(this, Variant());
	reset();
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
