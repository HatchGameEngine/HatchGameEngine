#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Scene.h>
#include <Engine/Types/Camera.h>

/***
* \class Camera
* \desc A basic camera entity.
*/

#ifdef SCRIPTABLE_ENTITY
ObjClass* Camera_Class = nullptr;

bool Camera_VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);
bool Camera_VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID);
#endif

/***
* \field Target
* \type Entity
* \default null
* \ns Camera
* \desc The camera's target.
*/
DECLARE_ENTITY_FIELD(Camera, Target);
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

void Camera::ClassLoad() {
	REGISTER_ENTITY(Camera);
	REGISTER_ENTITY_FIELD(Camera, Target);
	REGISTER_ENTITY_FIELD(Camera, ViewIndex);
	REGISTER_ENTITY_FIELD(Camera, UseBounds);
}
void Camera::ClassUnload() {
	UNREGISTER_ENTITY(Camera);
}

Entity* Camera::Spawn() {
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

	Target = nullptr;
	ViewIndex = 0;
	UseBounds = true;

	// TODO: Add getters and setters for the following
	MinX = 0.0;
	MinY = 0.0;
	MaxX = 0.0;
	MaxY = 0.0;

	if (Scene::Layers.size() > 0) {
		MaxX = Scene::Layers[0].Width * Scene::TileWidth;
		MaxY = Scene::Layers[0].Height * Scene::TileHeight;
	}

#ifdef SCRIPTABLE_ENTITY
	RunInitializer();
#endif
}

void Camera::MoveToTarget() {
	if (!Target || !Target->Active) {
		return;
	}

	int viewWidth = Scene::Views[ViewIndex].Width;
	int viewHeight = Scene::Views[ViewIndex].Height;

	float x = Target->X - (viewWidth / 2.0);
	float y = Target->Y - (viewHeight / 2.0);

	if (UseBounds) {
		x = Math::Clamp(x, MinX, MaxX - viewWidth);
		y = Math::Clamp(y, MinY, MaxY - viewHeight);
	}

	Scene::Views[ViewIndex].X = x;
	Scene::Views[ViewIndex].Y = y;
}

void Camera::Update() {
	if (!Active) {
		return;
	}

#ifdef SCRIPTABLE_ENTITY
	if (RunFunction(Hash_Update)) {
		return;
	}
#endif

	MoveToTarget();
}
void Camera::FixedUpdate() {
	if (!Active) {
		return;
	}

#ifdef SCRIPTABLE_ENTITY
	if (RunFunction(FixedUpdateHash)) {
		return;
	}
#endif

	if (Application::UseFixedTimestep) {
		MoveToTarget();
	}
}

#ifdef SCRIPTABLE_ENTITY
VMValue Camera_FieldGet_Target(Camera* camera, Uint32 threadID) {
	if (camera->Target) {
		return OBJECT_VAL(((ScriptEntity*)camera->Target)->Instance);
	}
	else {
		return NULL_VAL;
	}
}
void Camera_FieldSet_Target(Camera* camera, VMValue value, Uint32 threadID) {
	if (IS_NULL(value)) {
		camera->Target = nullptr;
	}
	else if (IS_ENTITY(value)) {
		camera->Target = (Entity*)AS_ENTITY(value)->EntityPtr;
	}
	else {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Target must be an Entity!");
	}
}

VMValue Camera_FieldGet_ViewIndex(Camera* camera, Uint32 threadID) {
	return INTEGER_VAL(camera->ViewIndex);
}
void Camera_FieldSet_ViewIndex(Camera* camera, VMValue value, Uint32 threadID) {
	if (!ScriptManager::DoIntegerConversion(value, threadID)) {
		return;
	}

	int viewIndex = AS_INTEGER(value);
	if (viewIndex >= 0 && viewIndex < MAX_SCENE_VIEWS) {
		camera->ViewIndex = viewIndex;
	}
	else {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false,
			"View index %d out of range. (%d - %d)",
			viewIndex,
			0,
			MAX_SCENE_VIEWS - 1);
	}
}

VMValue Camera_FieldGet_UseBounds(Camera* camera, Uint32 threadID) {
	return INTEGER_VAL(camera->UseBounds);
}
void Camera_FieldSet_UseBounds(Camera* camera, VMValue value, Uint32 threadID) {
	if (!ScriptManager::DoIntegerConversion(value, threadID)) {
		return;
	}

	camera->UseBounds = AS_INTEGER(value) ? true : false;
}

bool Camera_VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	Camera* entity = (Camera*)((ObjEntity*)object)->EntityPtr;

	ENTITY_GET_FIELD(Camera, Target);
	ENTITY_GET_FIELD(Camera, ViewIndex);
	ENTITY_GET_FIELD(Camera, UseBounds);

	return EntityImpl::VM_PropertyGet(object, hash, result, threadID);
}
bool Camera_VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	Camera* entity = (Camera*)((ObjEntity*)object)->EntityPtr;

	ENTITY_SET_FIELD(Camera, Target);
	ENTITY_SET_FIELD(Camera, ViewIndex);
	ENTITY_SET_FIELD(Camera, UseBounds);

	return EntityImpl::VM_PropertySet(object, hash, value, threadID);
}
#endif
