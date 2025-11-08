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

	input_buffer.clear();
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

	GDVIRTUAL_CALL(_gather); // TODO: return if the call have failed?

	const TypedArray<NodePath> props = replica_config->get_properties();

	InputFrame *frame = memnew(InputFrame);
	frame->frame_id = ++_last_frame_id;

	for (const NodePath &prop : props) {
		bool valid = false;
		Variant v = get_indexed(prop.get_names(), &valid);
		ERR_CONTINUE_MSG(!valid, vformat("Property '%s' not found.", prop));
		frame->properties.insert(prop, v);
	}

	write_frame(frame);
}

void NetworkInput::replay() {
	ERR_FAIL_COND_MSG(replica_config.is_null(), "Inputs buffering is not configured. Set a valid NetworkInputReplicaConfig resource to enable input buffering.");
	const TypedArray<NodePath> props = replica_config->get_properties();

	if (input_buffer.data_left() == 0) {
		return; // TODO: replay the oldest
	}

	InputFrame *frame = input_buffer.read();

	ERR_FAIL_COND(!frame);

	for (const NodePath &prop : props) {
		HashMap<NodePath, Variant>::ConstIterator E = frame->properties.find(prop);
		ERR_CONTINUE_MSG(!E, vformat("Property '%s' not found in input frame.", prop));
		set_indexed(prop.get_names(), E->value);
	}
}

void NetworkInput::write_frame(InputFrame *p_frame) {
	ERR_FAIL_COND(!p_frame);

	if (input_buffer.space_left() == 0) {
		InputFrame *old = input_buffer.read();
		if (old) {
			memdelete(old);
		}
	}

	input_buffer.write(p_frame);
}

int NetworkInput::copy_buffer(InputFrame **p_buf, int p_count) const {
	p_count = MIN(p_count, input_buffer.data_left());
	return input_buffer.copy(p_buf, input_buffer.data_left() - p_count, p_count);
}

void NetworkInput::reset() {
	while (input_buffer.data_left() > 0) {
		InputFrame *frame = input_buffer.read();
		if (frame) {
			memdelete(frame);
		}
	}
	input_buffer.clear();
}

void NetworkInput::_bind_methods() {
	GDVIRTUAL_BIND(_gather);

	ClassDB::bind_method(D_METHOD("set_replica_config", "config"), &NetworkInput::set_replica_config);
	ClassDB::bind_method(D_METHOD("get_replica_config"), &NetworkInput::get_replica_config);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "replica_config", PROPERTY_HINT_RESOURCE_TYPE, "NetworkInputReplicaConfig", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_replica_config", "get_replica_config");
}

NetworkInput::NetworkInput() {
	input_buffer.resize(6); // 2^6 = 64 frames
}

NetworkInput::~NetworkInput() {
	reset();
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
