#ifndef ENGINE_TYPES_CAMERA_H
#define ENGINE_TYPES_CAMERA_H

#include <Engine/Bytecode/ScriptEntity.h>

class Camera : public ENTITY_PARENT_CLASS {
public:
	static void ClassLoad(void* manager);
	static void ClassUnload(void* manager);

	static Entity* Spawn(void* manager);

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
#ifdef SCRIPTABLE_ENTITY
	void MarkForGarbageCollection();
#endif
	void MoveToTarget();
	void MoveViewPosition();
	void UpdateLate();
	void FixedUpdateLate();
};

#endif /* ENGINE_TYPES_CAMERA_H */
