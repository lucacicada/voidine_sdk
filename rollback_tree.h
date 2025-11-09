#pragma once

#include "network.h"

#include "core/os/os.h"
#include "core/os/thread_safe.h"
#include "rollback_synchronizer.h"
#include "scene/main/scene_tree.h"

class RollbackTree : public SceneTree {
	_THREAD_SAFE_CLASS_

	GDCLASS(RollbackTree, SceneTree);

private:
	inline static RollbackTree *singleton = nullptr;

private:
	bool offline_and_sad = false;
	Vector<NetworkActor *> synchronizers;

public:
	static RollbackTree *get_singleton() { return singleton; }

	virtual int get_override_physics_steps() override;
	virtual void initialize() override;
	virtual void iteration_prepare() override;
	virtual void iteration_end() override;

	void add_synchronizer(NetworkActor *p_synchronizer) {
		ERR_FAIL_NULL(p_synchronizer);

		synchronizers.push_back(p_synchronizer);
	}
	void remove_synchronizer(NetworkActor *p_synchronizer) {
		ERR_FAIL_NULL(p_synchronizer);

		synchronizers.erase(p_synchronizer);
	}

	RollbackTree();
	~RollbackTree();
};
