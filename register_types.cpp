#include "register_types.h"

#include "circular_buffer.h"
#include "network.h"
#include "network_input.h"
#include "network_input_replica_config.h"
#include "rollback_multiplayer.h"
#include "rollback_replica_config.h"
#include "rollback_synchronizer.h"
#include "rollback_tree.h"

#ifdef TOOLS_ENABLED
#include "editor/network_input_editor_plugin.h"
#include "editor/voidine_editor_plugin.h"
#endif

static Network *_network = nullptr;

void initialize_voidine_sdk_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(CircularBuffer);

		GDREGISTER_CLASS(Network);

		GDREGISTER_CLASS(NetworkInput);
		GDREGISTER_CLASS(NetworkInputReplicaConfig);

		GDREGISTER_CLASS(RollbackTree);
		GDREGISTER_CLASS(RollbackMultiplayer);
		GDREGISTER_CLASS(RollbackSynchronizer);
		GDREGISTER_CLASS(RollbackReplicaConfig);
		if (GD_IS_CLASS_ENABLED(MultiplayerAPI)) {
			MultiplayerAPI::set_default_interface("RollbackMultiplayer");
		}

		// do not edit the default main loop type
		// ProjectSettings::get_singleton()->set("application/run/main_loop_type", "RollbackTree");

		_network = memnew(Network);
		Engine::get_singleton()->add_singleton(Engine::Singleton("Network", Network::get_singleton()));
	}
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<NetworkInputEditorPlugin>();
		EditorPlugins::add_by_type<VoidineEditorPlugin>();
	}
#endif
}

void uninitialize_voidine_sdk_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// Engine::get_singleton()->remove_singleton("Network");
		memdelete(_network);
	}
}
