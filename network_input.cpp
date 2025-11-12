#include "network_input.h"
#include "network.h"

Error NetworkInput::copy_buffer(Vector<InputFrame> &frames, int p_count) {
	ERR_FAIL_COND_V(p_count < -1, ERR_INVALID_PARAMETER);

	p_count = p_count == -1 ? buffer.data_left() : MIN(p_count, buffer.data_left());

	frames.resize(p_count);
	if (p_count > 0) {
		int copied = buffer.copy(frames.ptrw(), buffer.data_left() - p_count, p_count);
		ERR_FAIL_COND_V(copied != p_count, ERR_BUG);
	}
	return OK;
}

void NetworkInput::set_replica_config(Ref<NetworkInputReplicaConfig> p_config) {
	replica_config = p_config;
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

void NetworkInput::sample(const NodePath &p_property, const Variant &p_value) {
	if (!_samples.has(p_property)) {
		InputSample sample;
		sample.accumulated = p_value;
		sample.samples = 1;
		_samples.insert(p_property, sample);
		return;
	}

	InputSample &sample = _samples[p_property];

	switch (p_value.get_type()) {
		case Variant::VECTOR2:
		case Variant::VECTOR3:
		case Variant::VECTOR4:
		case Variant::VECTOR2I:
		case Variant::VECTOR3I:
		case Variant::VECTOR4I:
		case Variant::INT:
		case Variant::FLOAT: {
			sample.accumulated = Variant::evaluate(Variant::OP_ADD, sample.accumulated, p_value);
			sample.samples++;
		} break;
		case Variant::BOOL: {
			sample.accumulated = sample.accumulated.operator bool() || p_value.operator bool();
			sample.samples = 1;
		} break;
		default: {
			sample.accumulated = p_value;
			sample.samples = 1;
		} break;
	}
}

void NetworkInput::gather() {
	for (const KeyValue<NodePath, InputSample> &E : _samples) {
		const NodePath &prop = E.key;
		const InputSample &sample = E.value;
		Variant averaged_value = sample.accumulated;
		if (sample.samples > 1) {
			switch (averaged_value.get_type()) {
				case Variant::VECTOR2:
				case Variant::VECTOR3:
				case Variant::VECTOR4:
				case Variant::VECTOR2I:
				case Variant::VECTOR3I:
				case Variant::VECTOR4I:
				case Variant::INT:
				case Variant::FLOAT: {
					averaged_value = Variant::evaluate(Variant::OP_DIVIDE, averaged_value, Variant(sample.samples));
				} break;
			}
		}
		set_indexed(prop.get_names(), averaged_value);
	}

	_samples.clear();

	++_current_frame_id; // actually make sure we advance the frame before sending the call to the virtual method

	GDVIRTUAL_CALL(_gather); // TODO: return if the call have failed?

	if (replica_config.is_null()) {
		return; // do not care if there is no config, we just wont replicate anything
	}

	InputFrame frame;
	frame.frame_id = _current_frame_id;

	const Vector<NodePath> props = replica_config->get_replica_properties();
	for (const NodePath &prop : props) {
		bool valid = false;
		Variant v = get_indexed(prop.get_names(), &valid);

		// we must fail here, since we need to know the exact property type to serialize it later
		// it is possible to pass in null for invalid properties and still make this work
		// but that would hide potential bugs in the configuration
		// the dev is responsible to set everything up correctly
		// input usually must be deterministic and fully known beforehand between server and clients
		ERR_FAIL_COND_MSG(!valid, vformat("Property '%s' not found.", prop));

		frame.properties.insert(prop, v);
	}

	write_frame(frame);

	GDVIRTUAL_CALL(_input_applied);
}

void NetworkInput::replay() {
	ERR_FAIL_COND(replica_config.is_null());
	const Vector<NodePath> props = replica_config->get_replica_properties();

	if (buffer.data_left() == 0) {
		return; // TODO: replay the oldest
	}

	InputFrame frame = buffer.read();
	ERR_FAIL_COND(frame.frame_id == 0);
	for (const NodePath &prop : props) {
		ERR_CONTINUE_MSG(!frame.properties.has(prop), vformat("Property '%s' not found in input frame.", prop));
		set_indexed(prop.get_names(), frame.properties[prop]);
	}

	GDVIRTUAL_CALL(_input_applied);
}

void NetworkInput::write_frame(const InputFrame &p_frame) {
	ERR_FAIL_COND_MSG(p_frame.frame_id == 0, "Input frame is uninitialized.");

	if (buffer.space_left() == 0) {
		buffer.advance_read(1);
	}
	buffer.write(p_frame);
	last_aknownedged_input_id = p_frame.frame_id;
}

void NetworkInput::reset() {
	_samples.clear();
	buffer.clear();
}

bool NetworkInput::is_input_authority() const {
	if (!is_inside_tree()) {
		return false;
	}

	// this method essentially check if the input is operational
	// a non-authoritative input cannot gather and practically act as an empty node

	// server can process every input
	// clients can process only local inputs

	Ref<MultiplayerAPI> api = get_multiplayer();
	return is_multiplayer_authority() || (api.is_valid() && (api->is_server()));
}

void NetworkInput::_bind_methods() {
	GDVIRTUAL_BIND(_gather);
	GDVIRTUAL_BIND(_input_applied);

	ClassDB::bind_method(D_METHOD("get_current_frame"), &NetworkInput::get_current_frame);

	ClassDB::bind_method(D_METHOD("set_replica_config", "config"), &NetworkInput::set_replica_config);
	ClassDB::bind_method(D_METHOD("get_replica_config"), &NetworkInput::get_replica_config);

	ClassDB::bind_method(D_METHOD("sample", "property", "value"), &NetworkInput::sample);
	ClassDB::bind_method(D_METHOD("is_input_authority"), &NetworkInput::is_input_authority);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "replica_config", PROPERTY_HINT_RESOURCE_TYPE, "NetworkInputReplicaConfig", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_replica_config", "get_replica_config");
}

NetworkInput::NetworkInput() {
	buffer.resize(6); // 2^6 = 64 frames
}

void NetworkInput::_start() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	reset();
	get_multiplayer()->object_configuration_add(this, this);
}

void NetworkInput::_stop() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	get_multiplayer()->object_configuration_remove(this, this);
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
