#include <Engine/Application.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Scene.h>

Uint32 ScriptEntity::FixedUpdateEarlyHash = 0;
Uint32 ScriptEntity::FixedUpdateHash = 0;
Uint32 ScriptEntity::FixedUpdateLateHash = 0;

#define LINK_INT(VAR) Instance->InstanceObj.Fields->Put(#VAR, INTEGER_LINK_VAL(&VAR))
#define LINK_DEC(VAR) Instance->InstanceObj.Fields->Put(#VAR, DECIMAL_LINK_VAL(&VAR))
#define LINK_BOOL(VAR) Instance->InstanceObj.Fields->Put(#VAR, INTEGER_LINK_VAL(&VAR))

#define ENTITY_FIELD(name) Uint32 ScriptEntity::Hash_##name = 0;
ENTITY_FIELDS_LIST
#undef ENTITY_FIELD

Entity* ScriptEntity::Spawn() {
	throw std::runtime_error("Cannot directly spawn ScriptEntity!");
}
Entity* ScriptEntity::SpawnNamed(const char* objectName) {
	// If this class is implemented natively, we can use Entity::SpawnNamed.
	// This works because all native entities descend from ScriptEntity.
	// Otherwise, this must be a script-side class, so we spawn a regular ScriptEntity.
	Entity* object = Entity::SpawnNamed(objectName);
	if (object) {
		return object;
	}

	if (!ScriptManager::LoadObjectClass(objectName)) {
		Log::Print(Log::LOG_ERROR, "Could not find class of %s!", objectName);
		return nullptr;
	}

	ScriptEntity* scriptEntity = new ScriptEntity;
	if (!ScriptEntity::SpawnForClass(scriptEntity, objectName)) {
		Log::Print(Log::LOG_ERROR, "Could not find class of %s!", objectName);
		delete scriptEntity;
		return nullptr;
	}

	return (Entity*)scriptEntity;
}
bool ScriptEntity::SpawnForClass(ScriptEntity* entity, const char* objectName) {
	ObjClass* klass = ScriptManager::GetObjectClass(objectName);
	if (!klass) {
		return false;
	}

	entity->Link(NewEntity(klass));

	return true;
}

void ScriptEntity::Init() {
#define ENTITY_FIELD(name) ScriptEntity::Hash_##name = Murmur::EncryptString(#name);
	ENTITY_FIELDS_LIST
#undef ENTITY_FIELD

	SetUseFixedTimestep(Application::UseFixedTimestep);
}

void ScriptEntity::Link(ObjEntity* entity) {
	Instance = entity;
	Instance->EntityPtr = this;

	LinkFields();
	AddEntityClassMethods();
}

void ScriptEntity::LinkFields() {
	/***
    * \field X
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The X position of the entity.
    */
	LINK_DEC(X);
	/***
    * \field Y
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The Y position of the entity.
    */
	LINK_DEC(Y);
	/***
    * \field Z
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The Z position of the entity.
    */
	LINK_DEC(Z);
	/***
    * \field SpeedX
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The horizontal velocity of the entity.
    */
	LINK_DEC(SpeedX);
	/***
    * \field SpeedY
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The vertical velocity of the entity.
    */
	LINK_DEC(SpeedY);
	/***
	* \field XSpeed
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc Alias for <ref Entity.SpeedX>.
	*/
	Instance->InstanceObj.Fields->Put("XSpeed", DECIMAL_LINK_VAL(&SpeedX));
	/***
	* \field YSpeed
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc Alias for <ref Entity.SpeedY>.
	*/
	Instance->InstanceObj.Fields->Put("YSpeed", DECIMAL_LINK_VAL(&SpeedY));
	/***
    * \field GroundSpeed
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The speed of the entity on the ground.
    */
	LINK_DEC(GroundSpeed);
	/***
    * \field GravitySpeed
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The gravity rate of the entity.
    */
	LINK_DEC(GravitySpeed);
	/***
	* \field Gravity
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc Alias for <ref Entity.GravitySpeed>.
	*/
	Instance->InstanceObj.Fields->Put("Gravity", DECIMAL_LINK_VAL(&GravitySpeed));
	/***
    * \field AutoPhysics
    * \type boolean
    * \default false
    * \ns Entity
    * \desc Whether <ref Entity.ApplyMotion> is automatically called for this entity.
    */
	LINK_INT(AutoPhysics);
	/***
    * \field Angle
    * \type integer
    * \default 0
    * \ns Entity
    * \desc The angle of the entity on the ground, within the range of `0x00` - `0xFF`.
    */
	LINK_INT(Angle);
	/***
    * \field AngleMode
    * \type integer
    * \default 0
    * \ns Entity
    * \desc The angle mode of the entity on the ground, within the range of `0` - `3`.
    */
	LINK_INT(AngleMode);
	/***
    * \field OnGround
    * \type boolean
    * \default false
    * \ns Entity
    * \desc Whether the entity is on the ground.
    */
	LINK_INT(OnGround);
	/***
	* \field Ground
	* \type boolean
	* \default false
	* \ns Entity
	* \desc Alias for <ref Entity.OnGround>.
	*/
	Instance->InstanceObj.Fields->Put("Ground", INTEGER_LINK_VAL(&OnGround));

	/***
    * \field ScaleX
    * \type decimal
    * \default 1.0
    * \ns Entity
    * \desc Used by <ref Draw.SpriteBasic> for scaling the entity's sprite horizontally.
    */
	LINK_DEC(ScaleX);
	/***
    * \field ScaleY
    * \type decimal
    * \default 1.0
    * \ns Entity
    * \desc Used by <ref Draw.SpriteBasic> for scaling the entity's sprite vertically.
    */
	LINK_DEC(ScaleY);
	/***
    * \field Rotation
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc Used by <ref Draw.SpriteBasic> for rotating the entity's sprite. See <ref Entity.RotationStyle>.
    */
	LINK_DEC(Rotation);
	/***
    * \field Alpha
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc A field that may be used in <ref Entity.Render> for changing the opacity of a sprite.
    */
	LINK_DEC(Alpha);
	/***
	* \field BlendMode
	* \type integer
	* \default BlendMode_NORMAL
	* \ns Entity
	* \desc A field that may be used in <ref Entity.Render> for changing the BlendMode of a sprite.
	*/
	LINK_INT(BlendMode);
	/***
    * \field Priority
    * \type integer
    * \default 0
    * \ns Entity
    * \desc The priority, or draw group, where this entity is located.
    */
	LINK_INT(Priority);
	/***
    * \field Depth
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The depth of the entity. Used for sorting entity draw order.
    */
	LINK_DEC(Depth);

	/***
    * \field Sprite
    * \type integer
    * \default -1
    * \ns Entity
    * \desc The sprite index of the entity.
    */
	LINK_INT(Sprite);
	/***
    * \field CurrentAnimation
    * \type integer
    * \default -1
    * \ns Entity
    * \desc The current sprite animation index of the entity.
    */
	LINK_INT(CurrentAnimation);
	/***
    * \field CurrentFrame
    * \type integer
    * \default -1
    * \ns Entity
    * \desc The current frame index of the entity's current animation.
    */
	LINK_INT(CurrentFrame);
	/***
    * \field CurrentFrameCount
    * \type integer
    * \default -1
    * \ns Entity
    * \desc The frame count of the entity's current animation.
    */
	LINK_INT(CurrentFrameCount);
	/***
    * \field AnimationSpeed
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The animation speed of the entity's animation.
    */
	LINK_DEC(AnimationSpeed);
	/***
    * \field AnimationTimer
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The animation timer of the entity's animation.
    */
	LINK_DEC(AnimationTimer);
	/***
    * \field AnimationFrameDuration
    * \type integer
    * \default 0
    * \ns Entity
    * \desc The duration of the entity's current animation frame.
    */
	LINK_INT(AnimationFrameDuration);
	/***
    * \field AnimationLoopIndex
    * \type integer
    * \default 0
    * \ns Entity
    * \desc The loop index of entity's current animation.
    */
	LINK_INT(AnimationLoopIndex);
	/***
	* \field RotationStyle
	* \type <ref ROTSTYLE_*>
	* \default ROTSTYLE_NONE
	* \ns Entity
	* \desc The rotation style to use when this entity's sprite is drawn by <ref Draw.SpriteBasic>.
	*/
	LINK_INT(RotationStyle);
	/***
    * \field AnimationSpeedMult
    * \type decimal
    * \default 1.0
    * \ns Entity
    * \desc The animation speed multiplier of the entity.
    */
	LINK_DEC(AnimationSpeedMult);
	/***
    * \field AnimationSpeedAdd
    * \type integer
    * \default 0
    * \ns Entity
    * \desc This value is added to the result of <ref Entity.AnimationSpeed> * <ref Entity.AnimationSpeedMult> when the entity is being animated.
    */
	LINK_INT(AnimationSpeedAdd);
	/***
    * \field PrevAnimation
    * \type integer
    * \default -1
    * \ns Entity
    * \desc The previous sprite animation index of the entity, if it was changed.
    */
	LINK_INT(PrevAnimation);
	/***
    * \field AutoAnimate
    * \type boolean
    * \default true
    * \ns Entity
    * \desc Whether <ref Entity.Animate> is automatically called for this entity.
    */
	LINK_INT(AutoAnimate);

	/***
    * \field OnScreen
    * \type boolean
    * \default true
    * \ns Entity
    * \desc See <ref Entity.InRange>.
    */
	LINK_INT(OnScreen);
	/***
    * \field WasOffScreen
    * \type boolean
    * \default false
    * \ns Entity
    * \desc Indicates if the entity was previously off-screen.
    */
	LINK_INT(WasOffScreen);
	/***
	* \field UpdateRegionW
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The horizontal on-screen range where the entity can update. If this is set to `0.0`, the entity will update regardless of the camera's horizontal position.
	*/
	Instance->InstanceObj.Fields->Put("UpdateRegionW", DECIMAL_LINK_VAL(&OnScreenHitboxW));
	/***
	* \field UpdateRegionH
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The vertical on-screen range where the entity can update. If this is set to `0.0`, the entity will update regardless of the camera's vertical position.
	*/
	Instance->InstanceObj.Fields->Put("UpdateRegionH", DECIMAL_LINK_VAL(&OnScreenHitboxH));
	/***
	* \field UpdateRegionTop
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The top on-screen range where the entity can update. If set to `0.0`, the entity will use its <ref Entity.UpdateRegionH> instead.
	*/
	Instance->InstanceObj.Fields->Put("UpdateRegionTop", DECIMAL_LINK_VAL(&OnScreenRegionTop));
	/***
	* \field UpdateRegionLeft
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The left on-screen range where the entity can update. If set to `0.0`, the entity will use its <ref Entity.UpdateRegionW> instead.
	*/
	Instance->InstanceObj.Fields->Put(
		"UpdateRegionLeft", DECIMAL_LINK_VAL(&OnScreenRegionLeft));
	/***
	* \field UpdateRegionRight
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The left on-screen range where the entity can update. If set to `0.0`, the entity will use its <ref Entity.UpdateRegionW> instead.
	*/
	Instance->InstanceObj.Fields->Put(
		"UpdateRegionRight", DECIMAL_LINK_VAL(&OnScreenRegionRight));
	/***
	* \field UpdateRegionBottom
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The bottom on-screen range where the entity can update. If set to `0.0`, the entity will use its <ref Entity.UpdateRegionH> instead.
	*/
	Instance->InstanceObj.Fields->Put(
		"UpdateRegionBottom", DECIMAL_LINK_VAL(&OnScreenRegionBottom));
	/***
	* \field OnScreenHitboxW
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc Alias for <ref Entity.UpdateRegionW>.
	*/
	LINK_DEC(OnScreenHitboxW);
	/***
	* \field OnScreenHitboxH
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc Alias for <ref Entity.UpdateRegionH>.
	*/
	LINK_DEC(OnScreenHitboxH);
	/***
	* \field RenderRegionW
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The horizontal on-screen range where the entity can render. If set to `0.0`, the entity will render regardless of the camera's horizontal position.
	*/
	LINK_DEC(RenderRegionW);
	/***
	* \field RenderRegionH
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The vertical on-screen range where the entity can render. If set to `0.0`, the entity will render regardless of the camera's vertical position.
	*/
	LINK_DEC(RenderRegionH);
	/***
	* \field RenderRegionTop
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The top on-screen range where the entity can render. If set to `0.0`, the entity will use its <ref Entity.RenderRegionH> instead.
	*/
	LINK_DEC(RenderRegionTop);
	/***
	* \field RenderRegionLeft
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The left on-screen range where the entity can render. If this and <ref Entity.RenderRegionRight> are set to `0.0`, the entity will use its <ref Entity.RenderRegionW> instead.
	*/
	LINK_DEC(RenderRegionLeft);
	/***
	* \field RenderRegionRight
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The right on-screen range where the entity can render. If this and <ref Entity.RenderRegionLeft> are set to `0.0`, the entity will use its <ref Entity.RenderRegionW> instead.
	*/
	LINK_DEC(RenderRegionRight);
	/***
	* \field RenderRegionBottom
	* \type decimal
	* \default 0.0
	* \ns Entity
	* \desc The bottom on-screen range where the entity can render. If set to `0.0`, the entity will use its <ref Entity.RenderRegionH> instead.
	*/
	LINK_DEC(RenderRegionBottom);
	/***
    * \field Visible
    * \type boolean
    * \default true
    * \ns Entity
    * \desc Whether the entity is visible.
    */
	LINK_INT(Visible);
	/***
    * \field ViewRenderFlag
    * \type bitfield
    * \default 0xFFFFFFFF
    * \ns Entity
    * \desc Which views the entity can render on. By default, this is on for every view.
    */
	LINK_INT(ViewRenderFlag);
	/***
    * \field ViewOverrideFlag
    * \type bitfield
    * \default 0
    * \ns Entity
    * \desc Bypasses each view's entity rendering toggle set by <ref Scene.SetObjectViewRender>.
    */
	LINK_INT(ViewOverrideFlag);

	/***
    * \field HitboxW
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The width of the hitbox.
    */
	Instance->InstanceObj.Fields->Put("HitboxW", DECIMAL_LINK_VAL(&Hitbox.Width));
	/***
    * \field HitboxH
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The height of the hitbox.
    */
	Instance->InstanceObj.Fields->Put("HitboxH", DECIMAL_LINK_VAL(&Hitbox.Height));
	/***
    * \field HitboxOffX
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The horizontal offset of the hitbox.
    */
	Instance->InstanceObj.Fields->Put("HitboxOffX", DECIMAL_LINK_VAL(&Hitbox.OffsetX));
	/***
    * \field HitboxOffY
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The vertical offset of the hitbox.
    */
	Instance->InstanceObj.Fields->Put("HitboxOffY", DECIMAL_LINK_VAL(&Hitbox.OffsetY));

	/***
    * \field HitboxLeft
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The left extent of the hitbox.
    */
	// See EntityImpl::VM_PropertyGet and EntityImpl::VM_PropertySet
	/***
    * \field HitboxTop
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The top extent of the hitbox.
    */
	// See EntityImpl::VM_PropertyGet and EntityImpl::VM_PropertySet
	/***
    * \field HitboxRight
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The right extent of the hitbox.
    */
	// See EntityImpl::VM_PropertyGet and EntityImpl::VM_PropertySet
	/***
    * \field HitboxBottom
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc The bottom extent of the hitbox.
    */
	// See EntityImpl::VM_PropertyGet and EntityImpl::VM_PropertySet

	/***
    * \field Direction
    * \type <ref FLIP_*>
    * \default FLIP_NONE
    * \ns Entity
    * \desc Indicates whether the entity is X/Y flipped.
    */
	LINK_INT(Direction);
	/***
	* \field FlipFlag
	* \type <ref FLIP_*>
	* \default FLIP_NONE
	* \ns Entity
	* \desc Alias for <ref Entity.Direction>.
	*/
	Instance->InstanceObj.Fields->Put("FlipFlag", INTEGER_LINK_VAL(&Direction));

	/***
    * \field SlotID
    * \type integer
    * \default -1
    * \ns Entity
    * \desc If this entity was spawned from a scene file, this field contains the slot ID in which it was placed. If not, this field contains the default value of `-1`.
    */
	LINK_INT(SlotID);

	/***
	* \field Filter
	* \type integer
	* \default 0xFF
	* \ns Entity
	* \desc If there is a scene list loaded, this checks to see whether the entity would spawn based on the scene's filter. See <ref Scene_Filter>.
	*/
	LINK_INT(Filter);

	/***
    * \field CollisionLayers
    * \type bitfield
    * \default 0
    * \ns Entity
    * \desc Which layers the entity is able to collide with.
    */
	LINK_INT(CollisionLayers);
	/***
    * \field CollisionPlane
    * \type integer
    * \default 0
    * \ns Entity
    * \desc The collision plane of the entity.
    */
	LINK_INT(CollisionPlane);
	/***
    * \field CollisionMode
    * \type <ref CMODE_*>
    * \default CMODE_FLOOR
    * \ns Entity
    * \desc The tile collision mode of the entity.
    */
	LINK_INT(CollisionMode);
	/***
    * \field TileCollisions
    * \type <ref TILECOLLISION_*>
    * \default TILECOLLISION_NONE
    * \ns Entity
    * \desc The direction of tile collisions for this entity.
    */
	LINK_INT(TileCollisions);

	/***
    * \field Activity
    * \type <ref ACTIVE_*>
    * \default ACTIVE_BOUNDS
    * \ns Entity
    * \desc The active status for this entity.
    */
	LINK_INT(Activity);
	/***
    * \field InRange
    * \type boolean
    * \default false
    * \ns Entity
    * \desc Whether this entity is within active range; see <ref Entity.Activity>.
    */
	LINK_INT(InRange);

	/***
    * \field SensorX
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc After a successful call to <ref TileCollision.Line>, this value will contain the horizontal position of where the entity collided.
    */
	LINK_DEC(SensorX);
	/***
    * \field SensorY
    * \type decimal
    * \default 0.0
    * \ns Entity
    * \desc After a successful call to <ref TileCollision.Line>, this value will contain the vertical position of where the entity collided.
    */
	LINK_DEC(SensorY);
	/***
    * \field SensorCollided
    * \type boolean
    * \default 0
    * \ns Entity
    * \desc After a successful call to <ref TileCollision.Line>, this value will be `true` if the entity collided with a tile, `false` otherwise.
    */
	LINK_INT(SensorCollided);
	/***
    * \field SensorAngle
    * \type integer
    * \default 0
    * \ns Entity
    * \desc After a successful call to <ref TileCollision.Line>, this value will contain the angle of the tile within the range of `0x00` - `0xFF`.
    */
	LINK_INT(SensorAngle);

	/***
    * \field Active
    * \type boolean
    * \default true
    * \ns Entity
    * \desc Whether the entity is active. If set to false, the entity is removed at the end of the frame.
    */
	LINK_BOOL(Active);
	/***
    * \field Pauseable
    * \type boolean
    * \default true
    * \ns Entity
    * \desc Whether the entity stops updating when the scene is paused.
    */
	LINK_BOOL(Pauseable);
	/***
    * \field Persistent
    * \type boolean
    * \default false
    * \ns Entity
    * \deprecated See <ref Entity.Persistence> instead.
    */
	Instance->InstanceObj.Fields->Put("Persistent", INTEGER_LINK_VAL(&Persistence));
	/***
    * \field Interactable
    * \type boolean
    * \default true
    * \ns Entity
    * \desc Whether the entity can be interacted with. If set to `false`, the entity will not be included in `with` iterations.
    */
	LINK_BOOL(Interactable);
	/***
    * \field Persistence
    * \type <ref Persistence_*>
    * \default Persistence_NONE
    * \ns Entity
    * \desc Whether the entity persists between scenes.
    */
	LINK_INT(Persistence);
}

#undef LINK_INT
#undef LINK_DEC
#undef LINK_BOOL

// This copies Entity's methods into the instance's fields.
// If there is a method in the class with the same name, it's not copied.
void ScriptEntity::AddEntityClassMethods() {
	HashMap<VMValue>* srcMethods = EntityImpl::Class->Methods;

	srcMethods->WithAll([this](Uint32 methodHash, VMValue value) -> void {
		if (!ScriptManager::ClassHasMethod(Instance->Object.Class, methodHash)) {
			Instance->InstanceObj.Fields->Put(methodHash, value);
		}
	});
}

void ScriptEntity::SetUseFixedTimestep(bool useFixedTimestep) {
	if (useFixedTimestep) {
		FixedUpdateEarlyHash = Hash_UpdateEarly;
		FixedUpdateHash = Hash_Update;
		FixedUpdateLateHash = Hash_UpdateLate;
	}
	else {
		FixedUpdateEarlyHash = Hash_FixedUpdateEarly;
		FixedUpdateHash = Hash_FixedUpdate;
		FixedUpdateLateHash = Hash_FixedUpdateLate;
	}
}

bool ScriptEntity::GetCallableValue(Uint32 hash, VMValue& value) {
	VMValue result;

	// Look for a field in the instance which may shadow a method.
	if (Instance->InstanceObj.Fields->GetIfExists(hash, &result)) {
		value = result;
		return true;
	}

	ObjClass* klass = Instance->Object.Class;
	if (ScriptManager::GetClassMethod((Obj*)Instance, klass, hash, &result)) {
		value = result;
		return true;
	}

	return false;
}
bool ScriptEntity::RunFunction(Uint32 hash) {
	if (!Instance) {
		return false;
	}

	// NOTE:
	// If the function doesn't exist, this is not an error VM side,
	// treat whatever we call from C++ as a virtual-like function.
	VMValue callable;
	if (!GetCallableValue(hash, callable)) {
		return false;
	}

	VMThread* thread = ScriptManager::Threads + 0;
	VMValue* stackTop = thread->StackTop;

	thread->Push(OBJECT_VAL(Instance));
	thread->InvokeForEntity(callable, 0);

	thread->StackTop = stackTop;

	return true;
}
bool ScriptEntity::RunCreateFunction(VMValue flag) {
	VMValue callable;
	if (!GetCallableValue(Hash_Create, callable)) {
		return false;
	}

	VMThread* thread = ScriptManager::Threads + 0;
	VMValue* stackTop = thread->StackTop;
	thread->Push(OBJECT_VAL(Instance));

	// Create cannot be a native currently, but when it can, this IS_FUNCTION check will be needed.
	if (IS_FUNCTION(callable) && AS_FUNCTION(callable)->Arity == 0) {
		thread->InvokeForEntity(callable, 0);
	}
	else {
		thread->Push(flag);
		thread->InvokeForEntity(callable, 1);
	}

	thread->StackTop = stackTop;

	return false;
}
bool ScriptEntity::RunInitializer() {
	if (!HasInitializer(Instance->Object.Class)) {
		return true;
	}

	VMThread* thread = ScriptManager::Threads + 0;

	VMValue* stackTop = thread->StackTop;

	// Pushes this instance into the stack so that 'this' can work
	thread->Push(OBJECT_VAL(Instance));
	thread->CallInitializer(Instance->Object.Class->Initializer);
	thread->Pop(); // Pops it

	thread->StackTop = stackTop;

	return true;
}

bool ScriptEntity::ChangeClass(const char* className) {
	if (!ScriptManager::ClassExists(className)) {
		return false;
	}

	if (!ScriptManager::LoadObjectClass(className)) {
		return false;
	}

	ObjClass* newClass = ScriptManager::GetObjectClass(className);
	if (!newClass) {
		return false;
	}

	ObjectList* objectList = Scene::GetObjectList(className);
	if (!objectList) {
		return false;
	}

	// Remove from the old list
	List->Remove(this);

	// Add to the new list
	List = objectList;
	List->Add(this);

	// Change the script-side class proper
	Instance->Object.Class = newClass;

	return true;
}

void ScriptEntity::Copy(ScriptEntity* other, bool copyClass) {
	CopyFields(other);

	if (copyClass) {
		other->ChangeClass(List->ObjectName);
	}

	// Copy properties
	HashMap<Property>* srcProperties = Properties;
	HashMap<Property>* destProperties = other->Properties;

	destProperties->WithAll([](Uint32 key, Property value) -> void {
		Property::Delete(value);
	});
	destProperties->Clear();

	srcProperties->WithAll([destProperties](Uint32 key, Property value) -> void {
		destProperties->Put(key, value);
	});
}

void ScriptEntity::CopyFields(ScriptEntity* other) {
	Entity::CopyFields(other);

	CopyVMFields(other);
}

void ScriptEntity::CopyVMFields(ScriptEntity* other) {
	Table* srcFields = Instance->InstanceObj.Fields;
	Table* destFields = other->Instance->InstanceObj.Fields;

	destFields->Clear();

	srcFields->WithAll([destFields](Uint32 key, VMValue value) -> void {
		// Don't copy linked fields, because they point to this entity's built-in fields
		if (value.Type != VAL_LINKED_INTEGER && value.Type != VAL_LINKED_DECIMAL) {
			destFields->Put(key, value);
		}
	});

	// Link the built-in fields again, since they have been removed
	other->LinkFields();

	// Also re-add Entity's methods
	other->AddEntityClassMethods();
}

// Events called from C++
void ScriptEntity::Initialize() {
	if (!Instance) {
		return;
	}

	Entity::Initialize();

	RunInitializer();
}
void ScriptEntity::Create(VMValue flag) {
	if (!Instance) {
		return;
	}

	Created = true;

	RunCreateFunction(flag);
	if (Sprite >= 0 && CurrentAnimation < 0) {
		SetAnimation(0, 0);
	}
}
void ScriptEntity::Create() {
	Create(INTEGER_VAL(0));
}
void ScriptEntity::PostCreate() {
	if (!Instance) {
		return;
	}

	PostCreated = true;

	RunFunction(Hash_PostCreate);
}
void ScriptEntity::UpdateEarly() {
	if (!Active) {
		return;
	}

	RunFunction(Hash_UpdateEarly);
}
void ScriptEntity::Update() {
	if (!Active) {
		return;
	}

	RunFunction(Hash_Update);
}
void ScriptEntity::UpdateLate() {
	if (!Active) {
		return;
	}

	RunFunction(Hash_UpdateLate);
}
void ScriptEntity::FixedUpdateEarly() {
	if (!Active) {
		return;
	}

	RunFunction(FixedUpdateEarlyHash);
}
void ScriptEntity::FixedUpdate() {
	if (!Active) {
		return;
	}

	RunFunction(FixedUpdateHash);
}
void ScriptEntity::FixedUpdateLate() {
	if (!Active) {
		return;
	}

	RunFunction(FixedUpdateLateHash);

	Entity::FixedUpdateLate();
}
void ScriptEntity::RenderEarly() {
	if (!Active) {
		return;
	}

	RunFunction(Hash_RenderEarly);
}
void ScriptEntity::Render() {
	if (!Active) {
		return;
	}

	RunFunction(Hash_Render);
}
void ScriptEntity::RenderLate() {
	if (!Active) {
		return;
	}

	RunFunction(Hash_RenderLate);
}
#define CAN_CALL_ENTITY_IMPL(fnName) \
	(!GetCallableValue(Hash_##fnName, callable) || \
		(IS_NATIVE_FUNCTION(callable) && \
			AS_NATIVE_FUNCTION(callable) == EntityImpl::VM_##fnName))
void ScriptEntity::SetAnimation(int animation, int frame) {
	VMValue callable;
	if (CAN_CALL_ENTITY_IMPL(SetAnimation)) {
		Entity::SetAnimation(animation, frame);
		return;
	}

	VMThread* thread = ScriptManager::Threads + 0;
	VMValue* stackTop = thread->StackTop;
	thread->Push(OBJECT_VAL(Instance));
	thread->Push(INTEGER_VAL(animation));
	thread->Push(INTEGER_VAL(frame));
	thread->InvokeForEntity(callable, 2);
	thread->StackTop = stackTop;
}
void ScriptEntity::ResetAnimation(int animation, int frame) {
	VMValue callable;
	if (CAN_CALL_ENTITY_IMPL(ResetAnimation)) {
		Entity::ResetAnimation(animation, frame);
		return;
	}

	VMThread* thread = ScriptManager::Threads + 0;
	VMValue* stackTop = thread->StackTop;
	thread->Push(OBJECT_VAL(Instance));
	thread->Push(INTEGER_VAL(animation));
	thread->Push(INTEGER_VAL(frame));
	thread->InvokeForEntity(callable, 2);
	thread->StackTop = stackTop;
}
#undef CAN_CALL_ENTITY_IMPL
void ScriptEntity::OnAnimationFinish() {
	RunFunction(Hash_OnAnimationFinish);
}
void ScriptEntity::OnSceneLoad() {
	if (!Active) {
		return;
	}

	RunFunction(Hash_OnSceneLoad);
}
void ScriptEntity::OnSceneRestart() {
	if (!Active) {
		return;
	}

	RunFunction(Hash_OnSceneRestart);
}
void ScriptEntity::GameStart() {
	RunFunction(Hash_GameStart);
}
void ScriptEntity::Remove() {
	if (Removed) {
		return;
	}
	if (!Instance) {
		return;
	}

	RunFunction(Hash_Dispose);

	Active = false;
	Removed = true;
}
void ScriptEntity::Dispose() {
	Entity::Dispose();
	if (Instance) {
		Instance = NULL;
	}
}
bool ScriptEntity::IsValid() {
	return Active && !Removed;
}
