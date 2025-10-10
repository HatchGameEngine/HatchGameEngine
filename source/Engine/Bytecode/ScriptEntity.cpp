#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/ResourceTypes/Resource.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Scene.h>

bool ScriptEntity::DisableAutoAnimate = false;

bool SavedHashes = false;

#define LINK_INT(VAR) Instance->InstanceObj.Fields->Put(#VAR, INTEGER_LINK_VAL(&VAR))
#define LINK_DEC(VAR) Instance->InstanceObj.Fields->Put(#VAR, DECIMAL_LINK_VAL(&VAR))
#define LINK_BOOL(VAR) Instance->InstanceObj.Fields->Put(#VAR, INTEGER_LINK_VAL(&VAR))

#define ENTITY_FIELD(name) Uint32 Hash_##name = 0;
ENTITY_FIELDS_LIST
#undef ENTITY_FIELD

void ScriptEntity::Link(ObjEntity* entity) {
	Instance = entity;
	Instance->EntityPtr = this;
	Properties = new HashMap<VMValue>(NULL, 4);

	if (!SavedHashes) {
#define ENTITY_FIELD(name) Hash_##name = Murmur::EncryptString(#name);
		ENTITY_FIELDS_LIST
#undef ENTITY_FIELD

		SavedHashes = true;
	}

	LinkFields();
	AddEntityClassMethods();
}

void ScriptEntity::LinkFields() {
	/***
    * \field X
    * \type Decimal
    * \ns Entity
    * \desc The X position of the entity.
    */
	LINK_DEC(X);
	/***
    * \field Y
    * \type Decimal
    * \ns Entity
    * \desc The Y position of the entity.
    */
	LINK_DEC(Y);
	/***
    * \field Z
    * \type Decimal
    * \ns Entity
    * \desc The Z position of the entity.
    */
	LINK_DEC(Z);
	/***
    * \field XSpeed
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The horizontal velocity of the entity.
    */
	LINK_DEC(XSpeed);
	/***
    * \field YSpeed
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The vertical velocity of the entity.
    */
	LINK_DEC(YSpeed);
	/***
    * \field GroundSpeed
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The speed of the entity on the ground.
    */
	LINK_DEC(GroundSpeed);
	/***
    * \field Gravity
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The gravity of the entity.
    */
	LINK_DEC(Gravity);
	/***
    * \field AutoPhysics
    * \type Boolean
    * \default false
    * \ns Entity
    * \desc Whether <linkto ref="entity.ApplyMotion"></linkto> is automatically called for this entity.
    */
	LINK_INT(AutoPhysics);
	/***
    * \field Angle
    * \type Integer
    * \default 0
    * \ns Entity
    * \desc The angle of the entity on the ground, within the range of <code>0x00</code> - <code>0xFF</code>.
    */
	LINK_INT(Angle);
	/***
    * \field AngleMode
    * \type Integer
    * \default 0
    * \ns Entity
    * \desc The angle mode of the entity on the ground, within the range of <code>0</code> - <code>3</code>.
    */
	LINK_INT(AngleMode);
	/***
    * \field Ground
    * \type Boolean
    * \default false
    * \ns Entity
    * \desc Whether the entity is on the ground.
    */
	LINK_INT(Ground);

	/***
    * \field ScaleX
    * \type Decimal
    * \default 1.0
    * \ns Entity
    * \desc A field that may be used in <code>Render</code> for scaling a sprite horizontally.
    */
	LINK_DEC(ScaleX);
	/***
    * \field ScaleY
    * \type Decimal
    * \default 1.0
    * \ns Entity
    * \desc A field that may be used in <code>Render</code> for scaling a sprite vertically.
    */
	LINK_DEC(ScaleY);
	/***
    * \field Rotation
    * \type Decimal (radians)
    * \default 0.0
    * \ns Entity
    * \desc A field that may be used in <code>Render</code> for rotating a sprite.
    */
	LINK_DEC(Rotation);
	/***
    * \field Alpha
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc A field that may be used in <code>Render</code> for changing the opacity of a sprite.
    */
	LINK_DEC(Alpha);
	/***
	* \field BlendMode
	* \type Integer
	* \default BlendMode_NORMAL
	* \ns Entity
	* \desc A field that may be used in <code>Render</code> for changing the BlendMode of a sprite.
	*/
	LINK_INT(BlendMode);
	/***
    * \field Priority
    * \type Integer
    * \default 0
    * \ns Entity
    * \desc The priority, or draw group, where this entity is located.
    */
	LINK_INT(Priority);
	/***
    * \field Depth
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The depth of the entity. Used for sorting entity draw order.
    */
	LINK_DEC(Depth);

	/***
    * \field Sprite
    * \type Resource
    * \default null
    * \ns Entity
    * \desc The sprite Resource of the entity.
    */
	// See ScriptEntity::VM_Getter and ScriptEntity::VM_Setter
	/***
    * \field CurrentAnimation
    * \type Integer
    * \default -1
    * \ns Entity
    * \desc The current sprite animation index of the entity.
    */
	LINK_INT(CurrentAnimation);
	/***
    * \field CurrentFrame
    * \type Integer
    * \default -1
    * \ns Entity
    * \desc The current frame index of the entity's current animation.
    */
	LINK_INT(CurrentFrame);
	/***
    * \field CurrentFrameCount
    * \type Integer
    * \default -1
    * \ns Entity
    * \desc The frame count of the entity's current animation.
    */
	LINK_INT(CurrentFrameCount);
	/***
    * \field AnimationSpeed
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The animation speed of the entity's animation.
    */
	LINK_DEC(AnimationSpeed);
	/***
    * \field AnimationTimer
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The animation timer of the entity's animation.
    */
	LINK_DEC(AnimationTimer);
	/***
    * \field AnimationFrameDuration
    * \type Integer
    * \default 0
    * \ns Entity
    * \desc The duration of the entity's current animation frame.
    */
	LINK_INT(AnimationFrameDuration);
	/***
    * \field AnimationLoopIndex
    * \type Integer
    * \default 0
    * \ns Entity
    * \desc The loop index of entity's current animation.
    */
	LINK_INT(AnimationLoopIndex);
	/***
	* \field RotationStyle
	* \type Enumeration
	* \default ROTSTYLE_NONE
	* \ns Entity
	* \desc The rotation style to use when this entity is called in <linkto ref="Draw.SpriteBasic"></linkto>.
	*/
	LINK_INT(AnimationLoopIndex);
	/***
    * \field AnimationSpeedMult
    * \type Decimal
    * \default 1.0
    * \ns Entity
    * \desc The animation speed multiplier of the entity.
    */
	LINK_DEC(AnimationSpeedMult);
	/***
    * \field AnimationSpeedAdd
    * \type Integer
    * \default 0
    * \ns Entity
    * \desc This value is added to the result of <linkto ref="entity.AnimationSpeed"></linkto> * <linkto ref="entity.AnimationSpeedMult"></linkto> when the entity is being animated.
    */
	LINK_INT(AnimationSpeedAdd);
	/***
    * \field PrevAnimation
    * \type Integer
    * \default -1
    * \ns Entity
    * \desc The previous sprite animation index of the entity, if it was changed.
    */
	LINK_INT(PrevAnimation);
	/***
    * \field AutoAnimate
    * \type Boolean
    * \default true
    * \ns Entity
    * \desc Whether <linkto ref="entity.Animate"></linkto> is automatically called for this entity.
    */
	LINK_INT(AutoAnimate);

	/***
    * \field OnScreen
    * \type Boolean
    * \default true
    * \ns Entity
    * \desc See <linkto ref="entity.InRange"></linkto>.
    */
	LINK_INT(OnScreen);
	/***
    * \field WasOffScreen
    * \type Boolean
    * \default false
    * \ns Entity
    * \desc Indicates if the entity was previously off-screen.
    */
	LINK_INT(WasOffScreen);
	/***
    * \field OnScreenHitboxW
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc Alias for <linkto ref="entity.UpdateRegionW"></linkto>.
    */
	LINK_DEC(OnScreenHitboxW);
	/***
    * \field OnScreenHitboxH
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc Alias for <linkto ref="entity.UpdateRegionH"></linkto>.
    */
	LINK_DEC(OnScreenHitboxH);
	/***
    * \field Visible
    * \type Boolean
    * \default true
    * \ns Entity
    * \desc Whether the entity is visible or not.
    */
	LINK_INT(Visible);
	/***
    * \field ViewRenderFlag
    * \type Integer
    * \default ~0
    * \ns Entity
    * \desc A bitfield that indicates in which views the entity renders. By default, this is on for every view.
    */
	LINK_INT(ViewRenderFlag);
	/***
    * \field ViewOverrideFlag
    * \type Integer
    * \default 0
    * \ns Entity
    * \desc A bitfield similar to <linkto ref="entity.ViewRenderFlag"></linkto>. Bypasses each view's entity rendering toggle set by <linkto ref="Scene.SetObjectViewRender"></linkto>.
    */
	LINK_INT(ViewOverrideFlag);

	/***
    * \field UpdateRegionW
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The horizontal on-screen range where the entity can update. If this is set to <code>0.0</code>, the entity will update regardless of the camera's horizontal position.
    */
	Instance->InstanceObj.Fields->Put("UpdateRegionW", DECIMAL_LINK_VAL(&OnScreenHitboxW));
	/***
    * \field UpdateRegionH
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The vertical on-screen range where the entity can update. If this is set to <code>0.0</code>, the entity will update regardless of the camera's vertical position.
    */
	Instance->InstanceObj.Fields->Put("UpdateRegionH", DECIMAL_LINK_VAL(&OnScreenHitboxH));
	/***
    * \field UpdateRegionTop
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The top on-screen range where the entity can update. If set to <code>0.0</code>, the entity will use its <linkto ref="entity.UpdateRegionH">UpdateRegionH</linkto> instead.
    */
	Instance->InstanceObj.Fields->Put("UpdateRegionTop", DECIMAL_LINK_VAL(&OnScreenRegionTop));
	/***
    * \field UpdateRegionLeft
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The left on-screen range where the entity can update. If set to <code>0.0</code>, the entity will use its <linkto ref="entity.UpdateRegionW">UpdateRegionW</linkto> instead.
    */
	Instance->InstanceObj.Fields->Put(
		"UpdateRegionLeft", DECIMAL_LINK_VAL(&OnScreenRegionLeft));
	/***
    * \field UpdateRegionRight
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The left on-screen range where the entity can update. If set to <code>0.0</code>, the entity will use its <linkto ref="entity.UpdateRegionW">UpdateRegionW</linkto> instead.
    */
	Instance->InstanceObj.Fields->Put(
		"UpdateRegionRight", DECIMAL_LINK_VAL(&OnScreenRegionRight));
	/***
    * \field UpdateRegionBottom
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The bottom on-screen range where the entity can update. If set to <code>0.0</code>, the entity will use its <linkto ref="entity.UpdateRegionH">UpdateRegionH</linkto> instead.
    */
	Instance->InstanceObj.Fields->Put(
		"UpdateRegionBottom", DECIMAL_LINK_VAL(&OnScreenRegionBottom));
	/***
    * \field RenderRegionW
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The horizontal on-screen range where the entity can render. If set to <code>0.0</code>, the entity will render regardless of the camera's horizontal position.
    */
	LINK_DEC(RenderRegionW);
	/***
    * \field RenderRegionH
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The vertical on-screen range where the entity can render. If set to <code>0.0</code>, the entity will render regardless of the camera's vertical position.
    */
	LINK_DEC(RenderRegionH);
	/***
    * \field RenderRegionTop
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The top on-screen range where the entity can render. If set to <code>0.0</code>, the entity will use its <linkto ref="entity.RenderRegionH">RenderRegionH</linkto> instead.
    */
	LINK_DEC(RenderRegionTop);
	/***
    * \field RenderRegionLeft
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The left on-screen range where the entity can render. If this and <linkto ref="entity.RenderRegionRight"></linkto> are set to <code>0.0</code>, the entity will use its <linkto ref="entity.RenderRegionW">RenderRegionW</linkto> instead.
    */
	LINK_DEC(RenderRegionLeft);
	/***
    * \field RenderRegionRight
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The left on-screen range where the entity can render. If this and <linkto ref="entity.RenderRegionLeft"></linkto> are set to <code>0.0</code>, the entity will use its <linkto ref="entity.RenderRegionW">RenderRegionW</linkto> instead.
    */
	LINK_DEC(RenderRegionRight);
	/***
    * \field RenderRegionBottom
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The bottom on-screen range where the entity can render. If set to <code>0.0</code>, the entity will use its <linkto ref="entity.RenderRegionH">RenderRegionH</linkto> instead.
    */
	LINK_DEC(RenderRegionBottom);

	/***
    * \field HitboxW
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The width of the hitbox.
    */
	Instance->InstanceObj.Fields->Put("HitboxW", DECIMAL_LINK_VAL(&Hitbox.Width));
	/***
    * \field HitboxH
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The height of the hitbox.
    */
	Instance->InstanceObj.Fields->Put("HitboxH", DECIMAL_LINK_VAL(&Hitbox.Height));
	/***
    * \field HitboxOffX
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The horizontal offset of the hitbox.
    */
	Instance->InstanceObj.Fields->Put("HitboxOffX", DECIMAL_LINK_VAL(&Hitbox.OffsetX));
	/***
    * \field HitboxOffY
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The vertical offset of the hitbox.
    */
	Instance->InstanceObj.Fields->Put("HitboxOffY", DECIMAL_LINK_VAL(&Hitbox.OffsetY));

	/***
    * \field HitboxLeft
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The left extent of the hitbox.
    */
	// See EntityImpl::VM_PropertyGet and EntityImpl::VM_PropertySet
	/***
    * \field HitboxTop
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The top extent of the hitbox.
    */
	// See EntityImpl::VM_PropertyGet and EntityImpl::VM_PropertySet
	/***
    * \field HitboxRight
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The right extent of the hitbox.
    */
	// See EntityImpl::VM_PropertyGet and EntityImpl::VM_PropertySet
	/***
    * \field HitboxBottom
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc The bottom extent of the hitbox.
    */
	// See EntityImpl::VM_PropertyGet and EntityImpl::VM_PropertySet

	/***
    * \field FlipFlag
    * \type Integer
    * \default 0
    * \ns Entity
    * \desc Bitfield that indicates whether the entity is X/Y flipped.
    */
	LINK_INT(FlipFlag);

	/***
    * \field VelocityX
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc Similar to <linkto ref="entity.XSpeed"></linkto>.
    */
	LINK_DEC(VelocityX);
	/***
    * \field VelocityX
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc Similar to <linkto ref="entity.YSpeed"></linkto>.
    */
	LINK_DEC(VelocityY);
	/***
    * \field GroundVel
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc Similar to <linkto ref="entity.GroundSpeed"></linkto>.
    */
	LINK_DEC(GroundVel);
	/***
    * \field Direction
    * \type Integer
    * \default 0
    * \ns Entity
    * \desc Similar to <linkto ref="entity.FlipFlag"></linkto>.
    */
	LINK_INT(Direction);
	/***
    * \field OnGround
    * \type Boolean
    * \default false
    * \ns Entity
    * \desc Similar to <linkto ref="entity.Ground"></linkto>.
    */
	LINK_INT(OnGround);

	/***
    * \field SlotID
    * \type Integer
    * \default -1
    * \ns Entity
    * \desc If this entity was spawned from a scene file, this field contains the slot ID in which it was placed. If not, this field contains the default value of <code>-1</code>.
    */
	LINK_INT(SlotID);

	/***
	* \field Filter
	* \type Integer
	* \default 0xFF
	* \ns Entity
	* \desc If there is a scene list loaded, this checks to see whether the entity would spawn based on the scene's filter. Defaults to <code>0xFF</code>.
	*/
	LINK_INT(Filter);

	/***
    * \field ZDepth
    * \type Decimal
    * \default 0.0
    * \ns Entity
    */
	LINK_DEC(ZDepth);

	/***
    * \field CollisionLayers
    * \type Integer
    * \default 0
    * \ns Entity
    * \desc A bitfield containing which layers an entity is able to collide with.
    */
	LINK_INT(CollisionLayers);
	/***
    * \field CollisionPlane
    * \type Integer
    * \default 0
    * \ns Entity
    */
	LINK_INT(CollisionPlane);
	/***
    * \field CollisionMode
    * \type Integer
    * \default 0
    * \ns Entity
    */
	LINK_INT(CollisionMode);
	/***
    * \field TileCollisions
    * \type Integer
    * \default 0
    * \ns Entity
    */
	LINK_INT(TileCollisions);

	/***
    * \field Activity
    * \type Enumeration
    * \default ACTIVE_BOUNDS
    * \ns Entity
    * \desc The active status for this entity.
    */
	LINK_INT(Activity);
	/***
    * \field InRange
    * \type Boolean
    * \default false
    * \ns Entity
    * \desc Whether this entity is within active range; see <linkto ref="entity.Activity"></linkto>.
    */
	LINK_INT(InRange);

	/***
    * \field SensorX
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc After a successful call to <linkto ref="TileCollision.Line"></linkto>, this value will contain the horizontal position of where the entity collided.
    */
	LINK_DEC(SensorX);
	/***
    * \field SensorY
    * \type Decimal
    * \default 0.0
    * \ns Entity
    * \desc After a successful call to <linkto ref="TileCollision.Line"></linkto>, this value will contain the vertical position of where the entity collided.
    */
	LINK_DEC(SensorY);
	/***
    * \field SensorAngle
    * \type Boolean
    * \default 0
    * \ns Entity
    * \desc After a successful call to <linkto ref="TileCollision.Line"></linkto>, this value will be <code>true</code> if the entity collided with a tile, <code>false</code> otherwise.
    */
	LINK_INT(SensorCollided);
	/***
    * \field SensorAngle
    * \type Integer
    * \default 0
    * \ns Entity
    * \desc After a successful call to <linkto ref="TileCollision.Line"></linkto>, this value will contain the angle of the tile within the range of <code>0x00</code> - <code>0xFF</code>.
    */
	LINK_INT(SensorAngle);

	/***
    * \field Active
    * \type Boolean
    * \default true
    * \ns Entity
    * \desc Whether the entity is active. If set to false, the entity is removed at the end of the frame.
    */
	LINK_BOOL(Active);
	/***
    * \field Pauseable
    * \type Boolean
    * \default true
    * \ns Entity
    * \desc Whether the entity stops updating when the scene is paused.
    */
	LINK_BOOL(Pauseable);
	/***
    * \field Persistent
    * \type Boolean
    * \default false
    * \ns Entity
    * \desc See <linkto ref="entity.Persistence"></linkto> instead.
    */
	Instance->InstanceObj.Fields->Put("Persistent", INTEGER_LINK_VAL(&Persistence));
	/***
    * \field Interactable
    * \type Boolean
    * \default true
    * \ns Entity
    * \desc Whether the entity can be interacted with. If set to <code>false</code>, the entity will not be included in <code>with</code> iterations.
    */
	LINK_BOOL(Interactable);
	/***
    * \field Persistence
    * \type Integer
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

	if (!ScriptManager::Classes->Exists(className) &&
		!ScriptManager::LoadObjectClass(className)) {
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
	HashMap<VMValue>* srcProperties = Properties;
	HashMap<VMValue>* destProperties = other->Properties;

	destProperties->Clear();

	srcProperties->WithAll([destProperties](Uint32 key, VMValue value) -> void {
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

	// Set defaults
	Active = true;
	Pauseable = true;
	Activity = ACTIVE_BOUNDS;
	InRange = false;

	XSpeed = 0.0f;
	YSpeed = 0.0f;
	GroundSpeed = 0.0f;
	Gravity = 0.0f;
	Ground = false;

	WasOffScreen = false;
	OnScreen = true;
	OnScreenHitboxW = 0.0f;
	OnScreenHitboxH = 0.0f;
	OnScreenRegionTop = 0.0f;
	OnScreenRegionLeft = 0.0f;
	OnScreenRegionRight = 0.0f;
	OnScreenRegionBottom = 0.0f;
	Visible = true;
	ViewRenderFlag = 0xFFFFFFFF;
	ViewOverrideFlag = 0;
	RenderRegionW = 0.0f;
	RenderRegionH = 0.0f;
	RenderRegionTop = 0.0f;
	RenderRegionLeft = 0.0f;
	RenderRegionRight = 0.0f;
	RenderRegionBottom = 0.0f;

	Angle = 0;
	AngleMode = 0;
	Rotation = 0.0;
	AutoPhysics = false;

	Priority = 0;
	PriorityListIndex = -1;
	PriorityOld = -1;

	if (Sprite != nullptr) {
		Resource::Release((ResourceType*)Sprite);
		Sprite = nullptr;
	}
	CurrentAnimation = -1;
	CurrentFrame = -1;
	CurrentFrameCount = 0;
	AnimationSpeedMult = 1.0;
	AnimationSpeedAdd = 0;
	AutoAnimate = ScriptEntity::DisableAutoAnimate ? false : true;
	AnimationSpeed = 0;
	AnimationTimer = 0.0;
	AnimationFrameDuration = 0;
	AnimationLoopIndex = 0;
	RotationStyle = ROTSTYLE_NONE;

	Hitbox.Clear();
	FlipFlag = 0;

	VelocityX = 0.0f;
	VelocityY = 0.0f;
	GroundVel = 0.0f;
	GravityStrength = 0.0f;
	OnGround = false;
	Direction = 0;

	Persistence = Persistence_NONE;
	Interactable = true;

	SetUpdatePriority(0);

	RunInitializer();
}
void ScriptEntity::Create(VMValue flag) {
	if (!Instance) {
		return;
	}

	Created = true;

	RunCreateFunction(flag);
	if (Sprite != nullptr && CurrentAnimation < 0) {
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

	if (AutoAnimate) {
		Animate();
	}
	if (AutoPhysics) {
		ApplyMotion();
	}
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

	if (RunFunction(Hash_Render)) {
		// Default render
	}
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

	SetSprite(nullptr);
}
void ScriptEntity::Dispose() {
	Entity::Dispose();
	if (Properties) {
		delete Properties;
	}
	if (Instance) {
		Instance = NULL;
	}
}
bool ScriptEntity::IsValid() {
	return Active && !Removed;
}
