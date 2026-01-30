#ifndef ENGINE_TYPES_CAMERA_H
#define ENGINE_TYPES_CAMERA_H

#include <Engine/Bytecode/ScriptEntity.h>

class Camera : public ENTITY_PARENT_CLASS {
public:
	static void ClassLoad();
	static void ClassUnload();

	static Entity* Spawn();

	Entity* TargetEntity;
	int ViewIndex;
	bool UseBounds;
	float MinX;
	float MinY;
	float MinZ;
	float MaxX;
	float MaxY;
	float MaxZ;

	void Initialize();
	void MoveToTarget();
	void MoveViewPosition();
	void Update();
	void FixedUpdate();
};

#endif /* ENGINE_TYPES_CAMERA_H */
