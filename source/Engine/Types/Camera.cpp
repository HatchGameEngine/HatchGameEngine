#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Scene.h>
#include <Engine/Types/Camera.h>

/***
* \class Camera
* \desc A basic camera entity.
*/

#ifdef SCRIPTABLE_ENTITY
ObjClass* Camera_Class = nullptr;

bool Camera_VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, VMThread* thread);
bool Camera_VM_PropertySet(Obj* object, Uint32 hash, VMValue value, VMThread* thread);
#endif

/***
* \field TargetEntity
* \type Entity
* \default null
* \ns Camera
* \desc The camera's target.
*/
DECLARE_ENTITY_FIELD(Camera, TargetEntity);
/***
* \field ViewIndex
* \type integer
* \default 0
* \ns Camera
* \desc The view that the camera operates on.
*/
DECLARE_ENTITY_FIELD(Camera, ViewIndex);
/***
* \field UseBounds
* \type boolean
* \default true
* \ns Camera
* \desc Whether to restrict the camera to the bounds of the scene.
*/
DECLARE_ENTITY_FIELD(Camera, UseBounds);

void Camera::ClassLoad(void* manager) {
	REGISTER_ENTITY(Camera, manager);
	REGISTER_ENTITY_FIELD(Camera, TargetEntity);
	REGISTER_ENTITY_FIELD(Camera, ViewIndex);
	REGISTER_ENTITY_FIELD(Camera, UseBounds);
}
void Camera::ClassUnload(void* manager) {
	UNREGISTER_ENTITY(Camera, manager);
}

Entity* Camera::Spawn(void* manager) {
	Camera* camera = new Camera;

#ifdef SCRIPTABLE_ENTITY
	if (!ENTITY_SPAWN(camera, Camera)) {
		delete camera;

		return nullptr;
	}
#endif

	REGISTER_ENTITY_GETTER(Camera, camera);
	REGISTER_ENTITY_SETTER(Camera, camera);

	return (Entity*)camera;
}

void Camera::Initialize() {
	Entity::Initialize();

	TargetEntity = nullptr;
	ViewIndex = 0;
	UseBounds = true;

	// TODO: Add getters and setters for the following
	MinX = 0.0;
	MinY = 0.0;
	MinZ = 0.0;
	MaxX = 0.0;
	MaxY = 0.0;
	MaxZ = 0.0;

	if (Scene::Layers.size() > 0) {
		MaxX = Scene::Layers[0].Width * Scene::TileWidth;
		MaxY = Scene::Layers[0].Height * Scene::TileHeight;
	}

#ifdef SCRIPTABLE_ENTITY
	RunInitializer();
#endif
}

#ifdef SCRIPTABLE_ENTITY
void Camera::MarkForGarbageCollection() {
	ScriptEntity::MarkForGarbageCollection();

	if (TargetEntity) {
		Manager->GC->GrayObject(((ScriptEntity*)TargetEntity)->Instance);
	}
}
#endif

void Camera::MoveViewPosition() {
	float x = X;
	float y = Y;
	float z = Z;

	if (UseBounds) {
		if (!Scene::Views[ViewIndex].UsePerspective) {
			x = Math::Clamp(x, MinX, MaxX - Scene::Views[ViewIndex].Width);
			y = Math::Clamp(y, MinY, MaxY - Scene::Views[ViewIndex].Height);
		}
		else {
			x = Math::Clamp(x, MinX, MaxX);
			y = Math::Clamp(y, MinY, MaxY);
		}

		z = Math::Clamp(z, MinZ, MaxZ);
	}

	Scene::Views[ViewIndex].X = x;
	Scene::Views[ViewIndex].Y = y;
	Scene::Views[ViewIndex].Z = z;
}

void Camera::MoveToTarget() {
	if (!TargetEntity || !TargetEntity->Active) {
		return;
	}

	X = TargetEntity->X;
	Y = TargetEntity->Y;
	Z = TargetEntity->Z;

	if (!Scene::Views[ViewIndex].UsePerspective) {
		X -= Scene::Views[ViewIndex].Width * 0.5;
		Y -= Scene::Views[ViewIndex].Height * 0.5;
	}
}

void Camera::UpdateLate() {
	if (!Active) {
		return;
	}

#ifdef SCRIPTABLE_ENTITY
	if (RunFunction(Hash_UpdateLate)) {
		return;
	}
#endif

	MoveToTarget();
	MoveViewPosition();
}
void Camera::FixedUpdateLate() {
	if (!Active) {
		return;
	}

#ifdef SCRIPTABLE_ENTITY
	if (RunFunction(FixedUpdateLateHash)) {
		return;
	}
#endif

	if (Application::UseFixedTimestep) {
		MoveToTarget();
		MoveViewPosition();
	}
}

#ifdef SCRIPTABLE_ENTITY
VMValue Camera_FieldGet_TargetEntity(Camera* camera, VMThread* thread) {
	if (camera->TargetEntity) {
		return OBJECT_VAL(((ScriptEntity*)camera->TargetEntity)->Instance);
	}
	else {
		return NULL_VAL;
	}
}
void Camera_FieldSet_TargetEntity(Camera* camera, VMValue value, VMThread* thread) {
	if (IS_NULL(value)) {
		camera->TargetEntity = nullptr;
	}
	else if (IS_ENTITY(value)) {
		camera->TargetEntity = (Entity*)AS_ENTITY(value)->EntityPtr;
	}
	else {
		throw ScriptException("TargetEntity must be an Entity!");
	}
}

VMValue Camera_FieldGet_ViewIndex(Camera* camera, VMThread* thread) {
	return INTEGER_VAL(camera->ViewIndex);
}
void Camera_FieldSet_ViewIndex(Camera* camera, VMValue value, VMThread* thread) {
	if (!thread->DoIntegerConversion(value)) {
		return;
	}

	int viewIndex = AS_INTEGER(value);
	if (viewIndex >= 0 && viewIndex < MAX_SCENE_VIEWS) {
		camera->ViewIndex = viewIndex;
	}
	else {
		thread->ThrowRuntimeError(false,
			"View index %d out of range. (%d - %d)",
			viewIndex,
			0,
			MAX_SCENE_VIEWS - 1);
	}
}

VMValue Camera_FieldGet_UseBounds(Camera* camera, VMThread* thread) {
	return INTEGER_VAL(camera->UseBounds);
}
void Camera_FieldSet_UseBounds(Camera* camera, VMValue value, VMThread* thread) {
	if (!thread->DoIntegerConversion(value)) {
		return;
	}

	camera->UseBounds = AS_INTEGER(value) ? true : false;
}

bool Camera_VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, VMThread* thread) {
	Camera* entity = (Camera*)((ObjEntity*)object)->EntityPtr;

	ENTITY_GET_FIELD(Camera, TargetEntity);
	ENTITY_GET_FIELD(Camera, ViewIndex);
	ENTITY_GET_FIELD(Camera, UseBounds);

	return EntityImpl::VM_PropertyGet(object, hash, result, thread);
}
bool Camera_VM_PropertySet(Obj* object, Uint32 hash, VMValue value, VMThread* thread) {
	Camera* entity = (Camera*)((ObjEntity*)object)->EntityPtr;

	ENTITY_SET_FIELD(Camera, TargetEntity);
	ENTITY_SET_FIELD(Camera, ViewIndex);
	ENTITY_SET_FIELD(Camera, UseBounds);

	return EntityImpl::VM_PropertySet(object, hash, value, thread);
}
#endif
