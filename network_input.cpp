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

void NetworkInput::gather() {
	ERR_FAIL_COND_MSG(replica_config.is_null(), "Input buffering is not configured. Set a valid NetworkInputReplicaConfig resource to enable input buffering.");

	GDVIRTUAL_CALL(_gather); // TODO: return if the call have failed?

	const TypedArray<NodePath> props = replica_config->get_properties();

	InputFrame frame;
	frame.frame_id = ++_last_frame_id;

	for (const NodePath &prop : props) {
		bool valid = false;
		Variant v = get_indexed(prop.get_names(), &valid);
		ERR_FAIL_COND_MSG(!valid, vformat("Property '%s' not found.", prop));
		frame.properties.insert(prop, v);
	}

	write_frame(frame);
}

void NetworkInput::replay() {
	ERR_FAIL_COND_MSG(replica_config.is_null(), "Input buffering is not configured. Set a valid NetworkInputReplicaConfig resource to enable input buffering.");
	const TypedArray<NodePath> props = replica_config->get_properties();

	if (buffer.data_left() == 0) {
		return; // TODO: replay the oldest
	}

	InputFrame frame = buffer.read();
	ERR_FAIL_COND(frame.frame_id == 0);
	for (const NodePath &prop : props) {
		ERR_CONTINUE_MSG(!frame.properties.has(prop), vformat("Property '%s' not found in input frame.", prop));
		set_indexed(prop.get_names(), frame.properties[prop]);
	}
}

void NetworkInput::write_frame(const InputFrame &p_frame) {
	ERR_FAIL_COND_MSG(p_frame.frame_id == 0, "Input frame is uninitialized.");

	if (buffer.space_left() == 0) {
		buffer.advance_read(1);
	}
	buffer.write(p_frame);
}

void NetworkInput::reset() {
	buffer.clear();
}

void NetworkInput::_bind_methods() {
	GDVIRTUAL_BIND(_gather);

	ClassDB::bind_method(D_METHOD("set_replica_config", "config"), &NetworkInput::set_replica_config);
	ClassDB::bind_method(D_METHOD("get_replica_config"), &NetworkInput::get_replica_config);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "replica_config", PROPERTY_HINT_RESOURCE_TYPE, "NetworkInputReplicaConfig", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT), "set_replica_config", "get_replica_config");
}

NetworkInput::NetworkInput() {
	buffer.resize(6);
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
