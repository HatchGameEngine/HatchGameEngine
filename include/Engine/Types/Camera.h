#ifndef ENGINE_BYTECODE_CAMERA_H
#define ENGINE_BYTECODE_CAMERA_H

#include <Engine/Bytecode/ScriptEntity.h>

class Camera : public ENTITY_PARENT_CLASS {
public:
	static void ClassLoad();
	static void ClassUnload();

	static Entity* Spawn();

	void MoveToTarget();

	Entity* Target;
	int ViewIndex;

	void Update();
	void FixedUpdate();
};

#endif /* ENGINE_BYTECODE_CAMERA_H */
