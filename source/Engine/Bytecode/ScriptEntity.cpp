#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Scene.h>

bool ScriptEntity::DisableAutoAnimate = false;

#define LINK_INT(VAR) Instance->Fields->Put(#VAR, INTEGER_LINK_VAL(&VAR))
#define LINK_DEC(VAR) Instance->Fields->Put(#VAR, DECIMAL_LINK_VAL(&VAR))
#define LINK_BOOL(VAR) Instance->Fields->Put(#VAR, INTEGER_LINK_VAL(&VAR))

bool   SavedHashes = false;
Uint32 Hash_Create = 0;
Uint32 Hash_PostCreate = 0;
Uint32 Hash_Update = 0;
Uint32 Hash_UpdateLate = 0;
Uint32 Hash_UpdateEarly = 0;
Uint32 Hash_RenderEarly = 0;
Uint32 Hash_Render = 0;
Uint32 Hash_RenderLate = 0;
Uint32 Hash_OnAnimationFinish = 0;
Uint32 Hash_OnSceneLoad = 0;
Uint32 Hash_OnSceneRestart = 0;
Uint32 Hash_GameStart = 0;
Uint32 Hash_Dispose = 0;
Uint32 Hash_HitboxLeft = 0;
Uint32 Hash_HitboxTop = 0;
Uint32 Hash_HitboxRight = 0;
Uint32 Hash_HitboxBottom = 0;

void ScriptEntity::Link(ObjInstance* instance) {
    Instance = instance;
    Instance->EntityPtr = this;
    Properties = new HashMap<VMValue>(NULL, 4);

    if (!SavedHashes) {
        Hash_Create = Murmur::EncryptString("Create");
        Hash_PostCreate = Murmur::EncryptString("PostCreate");
        Hash_Update = Murmur::EncryptString("Update");
        Hash_UpdateLate = Murmur::EncryptString("UpdateLate");
        Hash_UpdateEarly = Murmur::EncryptString("UpdateEarly");
        Hash_RenderEarly = Murmur::EncryptString("RenderEarly");
        Hash_Render = Murmur::EncryptString("Render");
        Hash_RenderLate = Murmur::EncryptString("RenderLate");
        Hash_OnAnimationFinish = Murmur::EncryptString("OnAnimationFinish");
        Hash_OnSceneLoad = Murmur::EncryptString("OnSceneLoad");
        Hash_OnSceneRestart = Murmur::EncryptString("OnSceneRestart");
        Hash_GameStart = Murmur::EncryptString("GameStart");
        Hash_Dispose = Murmur::EncryptString("Dispose");
        Hash_HitboxLeft = Murmur::EncryptString("HitboxLeft");
        Hash_HitboxTop = Murmur::EncryptString("HitboxTop");
        Hash_HitboxRight = Murmur::EncryptString("HitboxRight");
        Hash_HitboxBottom = Murmur::EncryptString("HitboxBottom");

        SavedHashes = true;
    }

    instance->PropertyGet = VM_Getter;
    instance->PropertySet = VM_Setter;

    LinkFields();
}

void ScriptEntity::LinkFields() {
    /***
    * \field X
    * \type Decimal
    * \ns Instance
    * \desc The X position of the entity.
    */
    LINK_DEC(X);
    /***
    * \field Y
    * \type Decimal
    * \ns Instance
    * \desc The Y position of the entity.
    */
    LINK_DEC(Y);
    /***
    * \field Z
    * \type Decimal
    * \ns Instance
    * \desc The Z position of the entity.
    */
    LINK_DEC(Z);
    /***
    * \field XSpeed
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The horizontal velocity of the entity.
    */
    LINK_DEC(XSpeed);
    /***
    * \field YSpeed
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The vertical velocity of the entity.
    */
    LINK_DEC(YSpeed);
    /***
    * \field GroundSpeed
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The speed of the entity on the ground.
    */
    LINK_DEC(GroundSpeed);
    /***
    * \field Gravity
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The gravity of the entity.
    */
    LINK_DEC(Gravity);
    /***
    * \field AutoPhysics
    * \type Boolean
    * \default false
    * \ns Instance
    * \desc Whether <linkto ref="instance.ApplyMotion"></linkto> is automatically called for this entity.
    */
    LINK_INT(AutoPhysics);
    /***
    * \field Angle
    * \type Integer
    * \default 0
    * \ns Instance
    * \desc The angle of the entity on the ground, within the range of <code>0x00</code> - <code>0xFF</code>.
    */
    LINK_INT(Angle);
    /***
    * \field AngleMode
    * \type Integer
    * \default 0
    * \ns Instance
    * \desc The angle mode of the entity on the ground, within the range of <code>0</code> - <code>3</code>.
    */
    LINK_INT(AngleMode);
    /***
    * \field Ground
    * \type Boolean
    * \default false
    * \ns Instance
    * \desc Whether the entity is on the ground.
    */
    LINK_INT(Ground);

    /***
    * \field ScaleX
    * \type Decimal
    * \default 1.0
    * \ns Instance
    * \desc A field that may be used in <linkto ref="instance.Render"></linkto> for scaling a sprite horizontally.
    */
    LINK_DEC(ScaleX);
    /***
    * \field ScaleY
    * \type Decimal
    * \default 1.0
    * \ns Instance
    * \desc A field that may be used in <linkto ref="instance.Render"></linkto> for scaling a sprite vertically.
    */
    LINK_DEC(ScaleY);
    /***
    * \field Rotation
    * \type Decimal (radians)
    * \default 0.0
    * \ns Instance
    * \desc A field that may be used in <linkto ref="instance.Render"></linkto> for rotating a sprite.
    */
    LINK_DEC(Rotation);
    /***
    * \field Alpha
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc A field that may be used in <linkto ref="instance.Render"></linkto> for changing the opacity of a sprite.
    */
    LINK_DEC(Alpha);
    /***
    * \field Priority
    * \type Integer
    * \default 0
    * \ns Instance
    * \desc The priority, or draw group, where this entity is located.
    */
    LINK_INT(Priority);
    /***
    * \field Depth
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The depth of the entity. Used for sorting entity draw order.
    */
    LINK_DEC(Depth);

    /***
    * \field Sprite
    * \type Integer (Resource)
    * \default -1
    * \ns Instance
    * \desc The sprite index of the entity.
    */
    LINK_INT(Sprite);
    /***
    * \field CurrentAnimation
    * \type Integer
    * \default -1
    * \ns Instance
    * \desc The current sprite  animation index of the entity.
    */
    LINK_INT(CurrentAnimation);
    /***
    * \field CurrentFrame
    * \type Integer
    * \default -1
    * \ns Instance
    * \desc The current frame index of the entity's current animation.
    */
    LINK_INT(CurrentFrame);
    /***
    * \field CurrentFrameCount
    * \type Integer
    * \default -1
    * \ns Instance
    * \desc The frame count of the entity's current animation.
    */
    LINK_INT(CurrentFrameCount);
    /***
    * \field AnimationSpeed
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The animation speed of the entity's animation.
    */
    LINK_DEC(AnimationSpeed);
    /***
    * \field AnimationTimer
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The animation timer of the entity's animation.
    */
    LINK_DEC(AnimationTimer);
    /***
    * \field AnimationFrameDuration
    * \type Integer
    * \default 0
    * \ns Instance
    * \desc The duration of the entity's current animation frame.
    */
    LINK_INT(AnimationFrameDuration);
    /***
    * \field AnimationLoopIndex
    * \type Integer
    * \default 0
    * \ns Instance
    * \desc The loop index of entity's current animation.
    */
    LINK_INT(AnimationLoopIndex);
    /***
    * \field AnimationSpeedMult
    * \type Decimal
    * \default 1.0
    * \ns Instance
    * \desc The animation speed multiplier of the entity.
    */
    LINK_DEC(AnimationSpeedMult);
    /***
    * \field AnimationSpeedAdd
    * \type Integer
    * \default 0
    * \ns Instance
    * \desc This value is added to the result of <linkto ref="instance.AnimationSpeed"></linkto> * <linkto ref="instance.AnimationSpeedMult"></linkto> when the entity is being animated.
    */
    LINK_INT(AnimationSpeedAdd);
    /***
    * \field AutoAnimate
    * \type Boolean
    * \default true
    * \ns Instance
    * \desc Whether <linkto ref="instance.Animate"></linkto> is automatically called for this entity.
    */
    LINK_INT(AutoAnimate);

    /***
    * \field OnScreen
    * \type Boolean
    * \default true
    * \ns Instance
    * \desc See <linkto ref="instance.InRange"></linkto>.
    */
    LINK_INT(OnScreen);
    /***
    * \field WasOffScreen
    * \type Boolean
    * \default false
    * \ns Instance
    * \desc Indicates if the entity was previously off-screen.
    */
    LINK_INT(WasOffScreen);
    /***
    * \field OnScreenHitboxW
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc Alias for <linkto ref="instance.UpdateRegionW"></linkto>.
    */
    LINK_DEC(OnScreenHitboxW);
    /***
    * \field OnScreenHitboxH
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc Alias for <linkto ref="instance.UpdateRegionH"></linkto>.
    */
    LINK_DEC(OnScreenHitboxH);
    /***
    * \field Visible
    * \type Boolean
    * \default true
    * \ns Instance
    * \desc Whether the entity is visible or not.
    */
    LINK_INT(Visible);
    /***
    * \field ViewRenderFlag
    * \type Integer
    * \default ~0
    * \ns Instance
    * \desc A bitfield that indicates in which views the entity renders. By default, this is on for every view.
    */
    LINK_INT(ViewRenderFlag);
    /***
    * \field ViewOverrideFlag
    * \type Integer
    * \default 0
    * \ns Instance
    * \desc A bitfield similar to <linkto ref="instance.ViewRenderFlag"></linkto>. Bypasses each view's entity rendering toggle set by <linkto ref="Scene.SetObjectViewRender"></linkto>.
    */
    LINK_INT(ViewOverrideFlag);

    /***
    * \field UpdateRegionW
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The horizontal on-screen range where the entity can update. If this is set to <code>0.0</code>, the entity will update regardless of the camera's horizontal position.
    */
    Instance->Fields->Put("UpdateRegionW", DECIMAL_LINK_VAL(&OnScreenHitboxW));
    /***
    * \field UpdateRegionH
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The vertical on-screen range where the entity can update. If this is set to <code>0.0</code>, the entity will update regardless of the camera's vertical position.
    */
    Instance->Fields->Put("UpdateRegionH", DECIMAL_LINK_VAL(&OnScreenHitboxH));
    /***
    * \field UpdateRegionTop
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The top on-screen range where the entity can update. If set to <code>0.0</code>, the entity will use its <linkto ref="instance.UpdateRegionH">UpdateRegionH</linkto> instead.
    */
    Instance->Fields->Put("UpdateRegionTop", DECIMAL_LINK_VAL(&OnScreenRegionTop));
    /***
    * \field UpdateRegionLeft
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The left on-screen range where the entity can update. If set to <code>0.0</code>, the entity will use its <linkto ref="instance.UpdateRegionW">UpdateRegionW</linkto> instead.
    */
    Instance->Fields->Put("UpdateRegionLeft", DECIMAL_LINK_VAL(&OnScreenRegionLeft));
    /***
    * \field UpdateRegionRight
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The left on-screen range where the entity can update. If set to <code>0.0</code>, the entity will use its <linkto ref="instance.UpdateRegionW">UpdateRegionW</linkto> instead.
    */
    Instance->Fields->Put("UpdateRegionRight", DECIMAL_LINK_VAL(&OnScreenRegionRight));
    /***
    * \field UpdateRegionBottom
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The bottom on-screen range where the entity can update. If set to <code>0.0</code>, the entity will use its <linkto ref="instance.UpdateRegionH">UpdateRegionH</linkto> instead.
    */
    Instance->Fields->Put("UpdateRegionBottom", DECIMAL_LINK_VAL(&OnScreenRegionBottom));
    /***
    * \field RenderRegionW
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The horizontal on-screen range where the entity can render. If set to <code>0.0</code>, the entity will render regardless of the camera's horizontal position.
    */
    LINK_DEC(RenderRegionW);
    /***
    * \field RenderRegionH
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The vertical on-screen range where the entity can render. If set to <code>0.0</code>, the entity will render regardless of the camera's vertical position.
    */
    LINK_DEC(RenderRegionH);
    /***
    * \field RenderRegionTop
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The top on-screen range where the entity can render. If set to <code>0.0</code>, the entity will use its <linkto ref="instance.RenderRegionH">RenderRegionH</linkto> instead.
    */
    LINK_DEC(RenderRegionTop);
    /***
    * \field RenderRegionLeft
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The left on-screen range where the entity can render. If this and <linkto ref="instance.RenderRegionRight"></linkto> are set to <code>0.0</code>, the entity will use its <linkto ref="instance.RenderRegionW">RenderRegionW</linkto> instead.
    */
    LINK_DEC(RenderRegionLeft);
    /***
    * \field RenderRegionRight
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The left on-screen range where the entity can render. If this and <linkto ref="instance.RenderRegionLeft"></linkto> are set to <code>0.0</code>, the entity will use its <linkto ref="instance.RenderRegionW">RenderRegionW</linkto> instead.
    */
    LINK_DEC(RenderRegionRight);
    /***
    * \field RenderRegionBottom
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The bottom on-screen range where the entity can render. If set to <code>0.0</code>, the entity will use its <linkto ref="instance.RenderRegionH">RenderRegionH</linkto> instead.
    */
    LINK_DEC(RenderRegionBottom);

    /***
    * \field HitboxW
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The width of the hitbox.
    */
    Instance->Fields->Put("HitboxW", DECIMAL_LINK_VAL(&Hitbox.Width));
    /***
    * \field HitboxH
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The height of the hitbox.
    */
    Instance->Fields->Put("HitboxH", DECIMAL_LINK_VAL(&Hitbox.Height));
    /***
    * \field HitboxOffX
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The horizontal offset of the hitbox.
    */
    Instance->Fields->Put("HitboxOffX", DECIMAL_LINK_VAL(&Hitbox.OffsetX));
    /***
    * \field HitboxOffY
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The vertical offset of the hitbox.
    */
    Instance->Fields->Put("HitboxOffY", DECIMAL_LINK_VAL(&Hitbox.OffsetY));

    /***
    * \field HitboxLeft
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The left extent of the hitbox.
    */
    // See ScriptEntity::VM_Getter and ScriptEntity::VM_Setter
    /***
    * \field HitboxTop
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The top extent of the hitbox.
    */
    // See ScriptEntity::VM_Getter and ScriptEntity::VM_Setter
    /***
    * \field HitboxRight
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The right extent of the hitbox.
    */
    // See ScriptEntity::VM_Getter and ScriptEntity::VM_Setter
    /***
    * \field HitboxBottom
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc The bottom extent of the hitbox.
    */
    // See ScriptEntity::VM_Getter and ScriptEntity::VM_Setter

    /***
    * \field FlipFlag
    * \type Integer
    * \default 0
    * \ns Instance
    * \desc Bitfield that indicates whether the entity is X/Y flipped.
    */
    LINK_INT(FlipFlag);

    /***
    * \field VelocityX
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc Similar to <linkto ref="instance.XSpeed"></linkto>.
    */
    LINK_DEC(VelocityX);
    /***
    * \field VelocityX
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc Similar to <linkto ref="instance.YSpeed"></linkto>.
    */
    LINK_DEC(VelocityY);
    /***
    * \field GroundVel
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc Similar to <linkto ref="instance.GroundSpeed"></linkto>.
    */
    LINK_DEC(GroundVel);
    /***
    * \field Direction
    * \type Integer
    * \default 0
    * \ns Instance
    * \desc Similar to <linkto ref="instance.FlipFlag"></linkto>.
    */
    LINK_INT(Direction);
    /***
    * \field OnGround
    * \type Boolean
    * \default false
    * \ns Instance
    * \desc Similar to <linkto ref="instance.Ground"></linkto>.
    */
    LINK_INT(OnGround);

    /***
    * \field SlotID
    * \type Integer
    * \default -1
    * \ns Instance
    * \desc If this entity was spawned from a scene file, this field contains the slot ID in which it was placed. If not, this field contains the default value of <code>-1</code>.
    */
    LINK_INT(SlotID);

    /***
    * \field ZDepth
    * \type Decimal
    * \default 0.0
    * \ns Instance
    */
    LINK_DEC(ZDepth);

    /***
    * \field CollisionLayers
    * \type Integer
    * \default 0
    * \ns Instance
    * \desc A bitfield containing which layers an entity is able to collide with.
    */
    LINK_INT(CollisionLayers);
    /***
    * \field CollisionPlane
    * \type Integer
    * \default 0
    * \ns Instance
    */
    LINK_INT(CollisionPlane);
    /***
    * \field CollisionMode
    * \type Integer
    * \default 0
    * \ns Instance
    */
    LINK_INT(CollisionMode);
     /***
    * \field TileCollisions
    * \type Integer
    * \default 0
    * \ns Instance
    */
    LINK_INT(TileCollisions);

    /***
    * \field Activity
    * \type Enumeration
    * \default ACTIVE_BOUNDS
    * \ns Instance
    * \desc The active status for this entity.
    */
    LINK_INT(Activity);
    /***
    * \field InRange
    * \type Boolean
    * \default false
    * \ns Instance
    * \desc Whether this entity is within active range; see <linkto ref="instance.Activity"></linkto>.
    */
    LINK_INT(InRange);

    /***
    * \field SensorX
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc After a successful call to <linkto ref="TileCollision.Line"></linkto>, this value will contain the horizontal position of where the entity collided.
    */
    LINK_DEC(SensorX);
    /***
    * \field SensorY
    * \type Decimal
    * \default 0.0
    * \ns Instance
    * \desc After a successful call to <linkto ref="TileCollision.Line"></linkto>, this value will contain the vertical position of where the entity collided.
    */
    LINK_DEC(SensorY);
    /***
    * \field SensorAngle
    * \type Boolean
    * \default 0
    * \ns Instance
    * \desc After a successful call to <linkto ref="TileCollision.Line"></linkto>, this value will be <code>true</code> if the entity collided with a tile, <code>false</code> otherwise.
    */
    LINK_INT(SensorCollided);
    /***
    * \field SensorAngle
    * \type Integer
    * \default 0
    * \ns Instance
    * \desc After a successful call to <linkto ref="TileCollision.Line"></linkto>, this value will contain the angle of the tile within the range of <code>0x00</code> - <code>0xFF</code>.
    */
    LINK_INT(SensorAngle);

    /***
    * \field Active
    * \type Boolean
    * \default true
    * \ns Instance
    * \desc Whether the entity is active. If set to false, the entity is removed at the end of the frame.
    */
    LINK_BOOL(Active);
    /***
    * \field Pauseable
    * \type Boolean
    * \default true
    * \ns Instance
    * \desc Whether the entity stops updating when the scene is paused.
    */
    LINK_BOOL(Pauseable);
    /***
    * \field Persistent
    * \type Boolean
    * \default false
    * \ns Instance
    * \desc See <linkto ref="instance.Persistence"></linkto> instead.
    */
    Instance->Fields->Put("Persistent", INTEGER_LINK_VAL(&Persistence));
    /***
    * \field Interactable
    * \type Boolean
    * \default true
    * \ns Instance
    * \desc Whether the entity can be interacted with. If set to <code>false</code>, the entity will not be included in <code>with</code> iterations.
    */
    LINK_BOOL(Interactable);
    /***
    * \field Persistence
    * \type Integer
    * \default Persistence_NONE
    * \ns Instance
    * \desc Whether the entity persists between scenes.
    */
    LINK_INT(Persistence);
}

#undef LINK_INT
#undef LINK_DEC
#undef LINK_BOOL

bool ScriptEntity::GetCallableValue(Uint32 hash, VMValue& value) {
    // First look for a field which may shadow a method.
    VMValue result;
    if (Instance->Fields->GetIfExists(hash, &result)) {
        value = result;
        return true;
    }

    ObjClass* klass = Instance->Object.Class;
    if (klass->Methods->GetIfExists(hash, &result)) {
        value = result;
        return true;
    }
    else {
        value = ScriptManager::GetClassMethod(klass, hash);
        if (!IS_NULL(value))
            return true;
    }

    return false;
}
ObjFunction* ScriptEntity::GetCallableFunction(Uint32 hash) {
    ObjClass* klass = Instance->Object.Class;
    VMValue result;
    if (klass->Methods->GetIfExists(hash, &result))
        return AS_FUNCTION(result);
    return NULL;
}
bool ScriptEntity::RunFunction(Uint32 hash) {
    if (!Instance)
        return false;

    // NOTE:
    // If the function doesn't exist, this is not an error VM side,
    // treat whatever we call from C++ as a virtual-like function.
    VMValue value;
    if (!ScriptEntity::GetCallableValue(hash, value))
        return true;

    VMThread* thread = ScriptManager::Threads + 0;

    VMValue* stackTop = thread->StackTop;

    thread->Push(OBJECT_VAL(Instance));
    thread->InvokeForEntity(value, 0);

    thread->StackTop = stackTop;

    return true;
}
bool ScriptEntity::RunCreateFunction(VMValue flag) {
    // NOTE:
    // If the function doesn't exist, this is not an error VM side,
    // treat whatever we call from C++ as a virtual-like function.
    ObjFunction* func = ScriptEntity::GetCallableFunction(Hash_Create);
    if (!func)
        return true;

    VMThread* thread = ScriptManager::Threads + 0;

    VMValue* stackTop = thread->StackTop;

    thread->Push(OBJECT_VAL(Instance));

    if (func->Arity == 0) {
        thread->RunEntityFunction(func, 0);
    }
    else {
        thread->Push(flag);
        thread->RunEntityFunction(func, 1);
    }

    thread->StackTop = stackTop;

    return false;
}
bool ScriptEntity::RunInitializer() {
    if (!HasInitializer(Instance->Object.Class))
        return true;

    VMThread* thread = ScriptManager::Threads + 0;

    VMValue* stackTop = thread->StackTop;

    thread->Push(OBJECT_VAL(Instance)); // Pushes this instance into the stack so that 'this' can work
    thread->CallInitializer(Instance->Object.Class->Initializer);
    thread->Pop(); // Pops it

    thread->StackTop = stackTop;

    return true;
}

bool ScriptEntity::ChangeClass(const char* className) {
    if (!ScriptManager::ClassExists(className))
        return false;

    if (!ScriptManager::Classes->Exists(className) && !ScriptManager::LoadObjectClass(className, true))
        return false;

    ObjClass* newClass = ScriptManager::GetObjectClass(className);
    if (!newClass)
        return false;

    ObjectList* objectList = Scene::GetObjectList(className);
    if (!objectList)
        return false;

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

    if (copyClass)
        other->ChangeClass(List->ObjectName);

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
    Table* srcFields = Instance->Fields;
    Table* destFields = other->Instance->Fields;

    destFields->Clear();

    srcFields->WithAll([destFields](Uint32 key, VMValue value) -> void {
        // Don't copy linked fields, because they point to this entity's built-in fields
        if (value.Type != VAL_LINKED_INTEGER && value.Type != VAL_LINKED_DECIMAL)
            destFields->Put(key, value);
    });

    // Link the built-in fields again, since they have been removed
    other->LinkFields();
}

// Events called from C++
void ScriptEntity::Initialize() {
    if (!Instance) return;

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

    Sprite = -1;
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

    RunInitializer();
}
void ScriptEntity::Create(VMValue flag) {
    if (!Instance) return;

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
    if (!Instance) return;

    PostCreated = true;

    RunFunction(Hash_PostCreate);
}
void ScriptEntity::UpdateEarly() {
    if (!Active) return;

    RunFunction(Hash_UpdateEarly);
}
void ScriptEntity::Update() {
    if (!Active) return;

    RunFunction(Hash_Update);
}
void ScriptEntity::UpdateLate() {
    if (!Active) return;

    RunFunction(Hash_UpdateLate);

    if (AutoAnimate)
        Animate();
    if (AutoPhysics)
        ApplyMotion();
}
void ScriptEntity::RenderEarly() {
    if (!Active) return;

    RunFunction(Hash_RenderEarly);
}
void ScriptEntity::Render(int CamX, int CamY) {
    if (!Active) return;

    if (RunFunction(Hash_Render)) {
        // Default render
    }
}
void ScriptEntity::RenderLate() {
    if (!Active) return;

    RunFunction(Hash_RenderLate);
}
void ScriptEntity::OnAnimationFinish() {
    RunFunction(Hash_OnAnimationFinish);
}
void ScriptEntity::OnSceneLoad() {
    if (!Active) return;

    RunFunction(Hash_OnSceneLoad);
}
void ScriptEntity::OnSceneRestart() {
    if (!Active) return;

    RunFunction(Hash_OnSceneRestart);
}
void ScriptEntity::GameStart() {
    RunFunction(Hash_GameStart);
}
void ScriptEntity::Remove() {
    if (Removed) return;
    if (!Instance) return;

    RunFunction(Hash_Dispose);

    Active = false;
    Removed = true;
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

// Events/methods called from VM
#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) (argIndex < argCount ? GET_ARG(argIndex, StandardLibrary::argFunction) : argDefault)
#define GET_ENTITY(argIndex) (GetScriptEntity(args, argIndex, threadID))
ScriptEntity* GetScriptEntity(Obj* object) {
    ObjInstance* entity = (ObjInstance*)object;
    if (!entity->EntityPtr)
        return nullptr;
    return (ScriptEntity*)entity->EntityPtr;
}
ScriptEntity* GetScriptEntity(VMValue* args, int index, Uint32 threadID) {
    ObjInstance* entity = GET_ARG(index, GetInstance);
    if (!entity->EntityPtr)
        return nullptr;
    return (ScriptEntity*)entity->EntityPtr;
}
bool IsValidEntity(Entity* self) {
    if (!self || !self->Active || self->Removed)
        return false;

    return true;
}
bool TestEntityCollision(Entity* other, Entity* self) {
    if (!IsValidEntity(other))
        return false;

    return self->CollideWithObject(other);
}
bool ScriptEntity::VM_Getter(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
    Entity* self = GetScriptEntity(object);
    if (self == nullptr)
        return false;

    if (hash == Hash_HitboxLeft) {
        if (result)
            *result = DECIMAL_VAL(self->Hitbox.GetLeft());
        return true;
    }
    else if (hash == Hash_HitboxTop) {
        if (result)
            *result = DECIMAL_VAL(self->Hitbox.GetTop());
        return true;
    }
    else if (hash == Hash_HitboxRight) {
        if (result)
            *result = DECIMAL_VAL(self->Hitbox.GetRight());
        return true;
    }
    else if (hash == Hash_HitboxBottom) {
        if (result)
            *result = DECIMAL_VAL(self->Hitbox.GetBottom());
        return true;
    }

    return false;
}
bool ScriptEntity::VM_Setter(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
    Entity* self = GetScriptEntity(object);
    if (self == nullptr)
        return false;

    if (hash == Hash_HitboxLeft) {
        if (ScriptManager::DoDecimalConversion(value, threadID))
            self->Hitbox.SetLeft(AS_DECIMAL(value));
        return true;
    }
    else if (hash == Hash_HitboxTop) {
        if (ScriptManager::DoDecimalConversion(value, threadID))
            self->Hitbox.SetTop(AS_DECIMAL(value));
        return true;
    }
    else if (hash == Hash_HitboxRight) {
        if (ScriptManager::DoDecimalConversion(value, threadID))
            self->Hitbox.SetRight(AS_DECIMAL(value));
        return true;
    }
    else if (hash == Hash_HitboxBottom) {
        if (ScriptManager::DoDecimalConversion(value, threadID))
            self->Hitbox.SetBottom(AS_DECIMAL(value));
        return true;
    }

    return false;
}
/***
 * \method SetAnimation
 * \desc Changes the current animation of the entity, if the animation index differs from the entity's current animation index.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index.
 * \ns Instance
 */
VMValue ScriptEntity::VM_SetAnimation(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 3);
    Entity* self = GET_ENTITY(0);
    int animation = GET_ARG(1, GetInteger);
    int frame = GET_ARG(2, GetInteger);

    if (!IsValidEntity(self))
        return NULL_VAL;

    ResourceType* resource = Scene::GetSpriteResource(self->Sprite);
    if (!resource) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Sprite is not set!", animation);
        return NULL_VAL;
    }

    ISprite* sprite = resource->AsSprite;
    if (!sprite) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Sprite is not set!", animation);
        return NULL_VAL;
    }

    if (!(animation >= 0 && (size_t)animation < sprite->Animations.size())) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Animation %d is not in bounds of sprite.", animation);
        return NULL_VAL;
    }
    if (!(frame >= 0 && (size_t)frame < sprite->Animations[animation].Frames.size())) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Frame %d is not in bounds of animation %d.", frame, animation);
        return NULL_VAL;
    }

    self->SetAnimation(animation, frame);
    return NULL_VAL;
}
/***
 * \method ResetAnimation
 * \desc Changes the current animation of the entity.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index.
 * \ns Instance
 */
VMValue ScriptEntity::VM_ResetAnimation(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 3);
    Entity* self = GET_ENTITY(0);
    int animation = GET_ARG(1, GetInteger);
    int frame = GET_ARG(2, GetInteger);

    if (!IsValidEntity(self))
        return NULL_VAL;

    ResourceType* resource = Scene::GetSpriteResource(self->Sprite);
    if (!resource) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Sprite %d does not exist!", self->Sprite);
        return NULL_VAL;
    }

    ISprite* sprite = resource->AsSprite;
    if (!sprite) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Sprite %d does not exist!", self->Sprite);
        return NULL_VAL;
    }

    if (!(animation >= 0 && (Uint32)animation < sprite->Animations.size())) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Animation %d is not in bounds of sprite.", animation);
        return NULL_VAL;
    }
    if (!(frame >= 0 && (Uint32)frame < sprite->Animations[animation].Frames.size())) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Frame %d is not in bounds of animation %d.", frame, animation);
        return NULL_VAL;
    }

    self->ResetAnimation(animation, frame);
    return NULL_VAL;
}
/***
 * \method Animate
 * \desc Animates the entity.
 * \ns Instance
 */
VMValue ScriptEntity::VM_Animate(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 1);
    Entity* self = GET_ENTITY(0);
    if (self)
        self->Animate();
    return NULL_VAL;
}

/***
 * \method SetUpdatePriority
 * \desc Sets the update priority of the entity.
 * \param priority (Integer): The update priority.
 * \ns Instance
 */
VMValue ScriptEntity::VM_SetUpdatePriority(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    Entity* self = GET_ENTITY(0);
    int priority = GET_ARG(1, GetInteger);

    if (self && self->UpdatePriority != priority) {
        self->UpdatePriority = priority;

        // If the scene is loading, NeedEntitySort is set to true,
        // so that the entities are sorted always and Scene::AddToScene
        // doesn't have to insert the entities in a sorted manner.
        if (Scene::Initializing || self->Created)
            Scene::NeedEntitySort = true;
    }

    return NULL_VAL;
}

/***
 * \method GetIDWithinClass
 * \desc Gets the ordered ID of the entity amongst other entities of the same type.
 * \return Returns an Integer value.
 * \ns Instance
 */
VMValue ScriptEntity::VM_GetIDWithinClass(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 1);
    Entity* self = GET_ENTITY(0);
    if (!self || !self->List)
        return NULL_VAL;

    Entity* other = self->List->EntityFirst;
    int num = 0;

    while (other) {
        if (self == other)
            break;

        num++;
        other = other->NextEntityInList;
    }

    return INTEGER_VAL(num);
}

/***
 * \method AddToRegistry
 * \desc Adds the entity to a registry.
 * \param registry (String): The registry name.
 * \ns Instance
 */
VMValue ScriptEntity::VM_AddToRegistry(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    Entity* self = GET_ENTITY(0);
    char*   registry = GET_ARG(1, GetString);

    if (!IsValidEntity(self))
        return NULL_VAL;

    ObjectRegistry* objectRegistry;
    if (!Scene::ObjectRegistries->Exists(registry)) {
        objectRegistry = new ObjectRegistry();
        Scene::ObjectRegistries->Put(registry, objectRegistry);
    }
    else {
        objectRegistry = Scene::ObjectRegistries->Get(registry);
    }

    objectRegistry->Add(self);

    return NULL_VAL;
}
/***
 * \method IsInRegistry
 * \desc Checks if the entity is in a registry.
 * \param registry (String): The registry name.
 * \return Returns a Boolean value.
 * \ns Instance
 */
VMValue ScriptEntity::VM_IsInRegistry(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    Entity* self = GET_ENTITY(0);
    char*   registry = GET_ARG(1, GetString);

    if (!IsValidEntity(self) || !Scene::ObjectRegistries->Exists(registry))
        return NULL_VAL;

    ObjectRegistry* objectRegistry = Scene::ObjectRegistries->Get(registry);

    return INTEGER_VAL(objectRegistry->Contains(self));
}
/***
 * \method RemoveFromRegistry
 * \desc Removes the entity from a registry.
 * \param registry (String): The registry name.
 * \ns Instance
 */
VMValue ScriptEntity::VM_RemoveFromRegistry(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    Entity* self = GET_ENTITY(0);
    char*   registry = GET_ARG(1, GetString);

    if (!IsValidEntity(self) || !Scene::ObjectRegistries->Exists(registry))
        return NULL_VAL;

    ObjectRegistry* objectRegistry = Scene::ObjectRegistries->Get(registry);

    objectRegistry->Remove(self);

    return NULL_VAL;
}
/***
 * \method ApplyMotion
 * \desc Applies gravity and velocities to the entity.
 * \ns Instance
 */
VMValue ScriptEntity::VM_ApplyMotion(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 1);
    Entity* self = GET_ENTITY(0);
    if (IsValidEntity(self))
        self->ApplyMotion();
    return NULL_VAL;
}
/***
 * \method InView
 * \desc Checks if the specified positions and ranges are within the specified view.
 * \param viewIndex (Integer): The view index.
 * \param x (Decimal): The X position.
 * \param y (Decimal): The Y position.
 * \param w (Decimal): The width.
 * \param h (Decimal): The height.
 * \return Returns <code>true</code> if the specified positions and ranges are within the specified view, <code>false</code> if otherwise.
 * \ns Instance
 */
VMValue ScriptEntity::VM_InView(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 6);
    // Entity* self = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    int   view   = GET_ARG(1, GetInteger);
    float x      = GET_ARG(2, GetDecimal);
    float y      = GET_ARG(3, GetDecimal);
    float w      = GET_ARG(4, GetDecimal);
    float h      = GET_ARG(5, GetDecimal);

    if (x + w >= Scene::Views[view].X &&
        y + h >= Scene::Views[view].Y &&
        x      < Scene::Views[view].X + Scene::Views[view].Width &&
        y      < Scene::Views[view].Y + Scene::Views[view].Height)
        return INTEGER_VAL(true);

    return INTEGER_VAL(false);
}
/***
 * \method CollidedWithObject
 * \desc Checks if the entity collided with another entity, or any entity of the specified class name.
 * \param other (Instance/String): The entity or class to collide with.
 * \return Returns the entity that was collided with, or <code>null</code> if it did not collide with any entity.
 * \ns Instance
 */
VMValue ScriptEntity::VM_CollidedWithObject(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);

    ScriptEntity* self = GET_ENTITY(0);
    if (!IsValidEntity(self))
        return NULL_VAL;

    if (IS_INSTANCE(args[1])) {
        ScriptEntity* other = GET_ENTITY(1);
        if (TestEntityCollision(other, self))
            return OBJECT_VAL(other->Instance);
        return NULL_VAL;
    }

    if (!Scene::ObjectLists) return NULL_VAL;
    if (!Scene::ObjectRegistries) return NULL_VAL;

    char* object = GET_ARG(1, GetString);
    if (!Scene::ObjectRegistries->Exists(object)) {
        if (!Scene::ObjectLists->Exists(object))
            return NULL_VAL;
    }

    if (self->Hitbox.Width == 0.0f || self->Hitbox.Height == 0.0f)
        return NULL_VAL;

    ScriptEntity* other = NULL;
    ObjectList* objectList = Scene::ObjectLists->Get(object);

    other = (ScriptEntity*)objectList->EntityFirst;
    for (Entity* next; other; other = (ScriptEntity*)next) {
        next = other->NextEntityInList;
        if (TestEntityCollision(other, self))
            return OBJECT_VAL(other->Instance);
    }

    return NULL_VAL;
}
/***
 * \method GetHitboxFromSprite
 * \desc Updates the entity's hitbox with the hitbox in the specified sprite's animation, frame and hitbox ID.
 * \param sprite (Sprite): The sprite.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index.
 * \param hitbox (Integer): The hitbox ID.
 * \ns Instance
 */
VMValue ScriptEntity::VM_GetHitboxFromSprite(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 5);
    ScriptEntity* self = GET_ENTITY(0);
    ISprite* sprite = GET_ARG(1, GetSprite);
    int animation   = GET_ARG(2, GetInteger);
    int frame       = GET_ARG(3, GetInteger);
    int hitbox      = GET_ARG(4, GetInteger);

    if (!IsValidEntity(self) || !sprite)
        return NULL_VAL;

    if (!(animation > -1 && (size_t)animation < sprite->Animations.size())) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Animation %d is not in bounds of sprite.", animation);
        return NULL_VAL;
    }
    if (!(frame > -1 && (size_t)frame < sprite->Animations[animation].Frames.size())) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Frame %d is not in bounds of animation %d.", frame, animation);
        return NULL_VAL;
    }

    AnimFrame frameO = sprite->Animations[animation].Frames[frame];

    if (!(hitbox > -1 && hitbox < frameO.BoxCount)) {
        // ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Hitbox %d is not in bounds of frame %d.", hitbox, frame);
        self->Hitbox.Clear();
    }
    else {
        self->Hitbox.Set(frameO.Boxes[hitbox]);
    }

    return NULL_VAL;
}
/***
 * \method ReturnHitboxFromSprite
 * \desc Gets the hitbox in the specified sprite's animation, frame and hitbox ID.
 * \param sprite (Sprite): The sprite.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index.
 * \param hitbox (Integer): The hitbox ID.
 * \return Returns an array containing the hitbox top, left, right and bottom sides in that order. 
 * \ns Instance
 */
VMValue ScriptEntity::VM_ReturnHitboxFromSprite(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 5);
    ScriptEntity* self    = GET_ENTITY(0);
    ISprite* sprite         = GET_ARG(1, GetSprite);
    int animation           = GET_ARG(2, GetInteger);
    int frame               = GET_ARG(3, GetInteger);
    int hitbox              = GET_ARG(4, GetInteger);

    if (!IsValidEntity(self) || !sprite)
        return NULL_VAL;

    if (!(animation > -1 && (size_t)animation < sprite->Animations.size())) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Animation %d is not in bounds of sprite.", animation);
        return NULL_VAL;
    }
    if (!(frame > -1 && (size_t)frame < sprite->Animations[animation].Frames.size())) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Frame %d is not in bounds of animation %d.", frame, animation);
        return NULL_VAL;
    }

    AnimFrame frameO = sprite->Animations[animation].Frames[frame];

    if (!(hitbox > -1 && hitbox < frameO.BoxCount)) {
        // ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Hitbox %d is not in bounds of frame %d.", hitbox, frame);
        return NULL_VAL;
    }

    CollisionBox box = frameO.Boxes[hitbox];
    ObjArray* array = NewArray();
    array->Values->push_back(DECIMAL_VAL((float)box.Left));
    array->Values->push_back(DECIMAL_VAL((float)box.Top));
    array->Values->push_back(DECIMAL_VAL((float)box.Right));
    array->Values->push_back(DECIMAL_VAL((float)box.Bottom));
    return OBJECT_VAL(array);
}

/***
 * \method CollideWithObject
 * \desc Does collision with another entity.
 * \param other (Instance): The other entity to check collision for.
 * \return Returns <code>true</code> if the entity collided, <code>false</code> if otherwise.
 * \ns Instance
 */
VMValue ScriptEntity::VM_CollideWithObject(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    ScriptEntity* self = GET_ENTITY(0);
    ScriptEntity* other = GET_ENTITY(1);
    if (!IsValidEntity(self) || !IsValidEntity(other))
        return NULL_VAL;
    return INTEGER_VAL(self->CollideWithObject(other));
}
/***
 * \method SolidCollideWithObject
 * \desc Does solid collision with another entity.
 * \param other (Instance): The other entity to check collision for.
 * \return Returns <code>true</code> if the entity collided, <code>false</code> if otherwise.
 * \ns Instance
 */
VMValue ScriptEntity::VM_SolidCollideWithObject(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 3);
    ScriptEntity* self = GET_ENTITY(0);
    ScriptEntity* other = GET_ENTITY(1);
    int flag = GET_ARG(2, GetInteger);
    if (!IsValidEntity(self) || !IsValidEntity(other))
        return NULL_VAL;
    return INTEGER_VAL(self->SolidCollideWithObject(other, flag));
}
/***
 * \method TopSolidCollideWithObject
 * \desc Does solid collision with another entity's top.
 * \param other (Instance): The other entity to check collision for.
 * \return Returns <code>true</code> if the entity collided, <code>false</code> if otherwise.
 * \ns Instance
 */
VMValue ScriptEntity::VM_TopSolidCollideWithObject(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 3);
    ScriptEntity* self = GET_ENTITY(0);
    ScriptEntity* other = GET_ENTITY(1);
    int flag = GET_ARG(2, GetInteger);
    if (!IsValidEntity(self) || !IsValidEntity(other))
        return NULL_VAL;
    return INTEGER_VAL(self->TopSolidCollideWithObject(other, flag));
}

VMValue ScriptEntity::VM_ApplyPhysics(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 1);
    ScriptEntity* self = GET_ENTITY(0);
    if (IsValidEntity(self))
        self->ApplyPhysics();
    return NULL_VAL;
}

/***
 * \method PropertyExists
 * \desc Checks if a property exists in the entity.
 * \param property (String): The property name.
 * \return Returns <code>true</code> if the property exists, <code>false</code> if otherwise.
 * \ns Instance
 */
VMValue ScriptEntity::VM_PropertyExists(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    ScriptEntity* self = GET_ENTITY(0);
    char* property = GET_ARG(1, GetString);
    if (self && self->Properties->Exists(property))
        return INTEGER_VAL(1);
    return INTEGER_VAL(0);
}
/***
 * \method PropertyGet
 * \desc Gets a property exists from the entity.
 * \param property (String): The property name.
 * \return Returns the property if it exists, and <code>null</code> if the property does not exist.
 * \ns Instance
 */
VMValue ScriptEntity::VM_PropertyGet(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    ScriptEntity* self = GET_ENTITY(0);
    char* property = GET_ARG(1, GetString);
    if (!self || !self->Properties->Exists(property))
        return NULL_VAL;
    return self->Properties->Get(property);
}

/***
 * \method SetViewVisibility
 * \desc Sets whether the entity is visible on a specific view.
 * \param viewIndex (Integer): The view index.
 * \param visible (Boolean): Whether the entity will be visible or not on the specified view.
 * \ns Instance
 */
VMValue ScriptEntity::VM_SetViewVisibility(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 3);
    ScriptEntity* self = GET_ENTITY(0);
    int viewIndex = GET_ARG(1, GetInteger);
    bool visible = GET_ARG(2, GetInteger);
    if (IsValidEntity(self)) {
        int flag = 1 << viewIndex;
        if (visible)
            self->ViewRenderFlag |= flag;
        else
            self->ViewRenderFlag &= ~flag;
    }
    return NULL_VAL;
}
/***
 * \method SetViewOverride
 * \desc Toggles the bypass for each view's entity rendering toggle set by <linkto ref="Scene.SetObjectViewRender"></linkto>.
 * \param viewIndex (Integer): The view index.
 * \param visible (Boolean): Whether the entity will always be visible or not on the specified view.
 * \ns Instance
 */
VMValue ScriptEntity::VM_SetViewOverride(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 3);
    ScriptEntity* self = GET_ENTITY(0);
    int viewIndex = GET_ARG(1, GetInteger);
    bool override = GET_ARG(2, GetInteger);
    if (IsValidEntity(self)) {
        int flag = 1 << viewIndex;
        if (override)
            self->ViewOverrideFlag |= flag;
        else
            self->ViewOverrideFlag &= ~flag;
    }
    return NULL_VAL;
}

/***
 * \method AddToDrawGroup
 * \desc Adds the entity into the specified draw group.
 * \param drawGroup (Integer): The draw group.
 * \ns Instance
 */
VMValue ScriptEntity::VM_AddToDrawGroup(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    ScriptEntity* self = GET_ENTITY(0);
    if (!IsValidEntity(self))
        return NULL_VAL;
    int drawGroup = GET_ARG(1, GetInteger);
    if (drawGroup >= 0 && drawGroup < Scene::PriorityPerLayer) {
        if (!Scene::PriorityLists[drawGroup].Contains(self))
            Scene::PriorityLists[drawGroup].Add(self);
    }
    else
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Draw group %d out of range. (0 - %d)", drawGroup, Scene::PriorityPerLayer - 1);
    return NULL_VAL;
}
/***
 * \method IsInDrawGroup
 * \desc Checks if the entity is in the specified draw group.
 * \param drawGroup (Integer): The draw group.
 * \return Returns <code>true</code> if the entity is in the specified draw group, <code>false</code> if otherwise.
 * \ns Instance
 */
VMValue ScriptEntity::VM_IsInDrawGroup(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    ScriptEntity* self = GET_ENTITY(0);
    if (!IsValidEntity(self))
        return INTEGER_VAL(false);
    int drawGroup = GET_ARG(1, GetInteger);
    if (drawGroup >= 0 && drawGroup < Scene::PriorityPerLayer)
        return INTEGER_VAL(!!(Scene::PriorityLists[drawGroup].Contains(self)));
    else
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Draw group %d out of range. (0 - %d)", drawGroup, Scene::PriorityPerLayer - 1);
    return NULL_VAL;
}
/***
 * \method RemoveFromDrawGroup
 * \desc Removes the entity from the specified draw group.
 * \param drawGroup (Integer): The draw group.
 * \ns Instance
 */
VMValue ScriptEntity::VM_RemoveFromDrawGroup(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    ScriptEntity* self = GET_ENTITY(0);
    if (!IsValidEntity(self))
        return NULL_VAL;
    int drawGroup = GET_ARG(1, GetInteger);
    if (drawGroup >= 0 && drawGroup < Scene::PriorityPerLayer)
        Scene::PriorityLists[drawGroup].Remove(self);
    else
        ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Draw group %d out of range. (0 - %d)", drawGroup, Scene::PriorityPerLayer - 1);
    return NULL_VAL;
}

/***
 * \method PlaySound
 * \desc Plays a sound once from the entity.
 * \param sound (Integer): The sound index to play.
 * \paramOpt panning (Decimal): Control the panning of the audio. -1.0 makes it sound in left ear only, 1.0 makes it sound in right ear, and closer to 0.0 centers it. (0.0 is the default.)
 * \paramOpt speed (Decimal): Control the speed of the audio. > 1.0 makes it faster, < 1.0 is slower, 1.0 is normal speed. (1.0 is the default.)
 * \paramOpt volume (Decimal): Controls the volume of the audio. 0.0 is muted, 1.0 is normal volume. (1.0 is the default.)
 * \return Returns the channel index where the sound began to play, or <code>-1</code> if no channel was available.
 * \ns Instance
 */
VMValue ScriptEntity::VM_PlaySound(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckAtLeastArgCount(argCount, 2);
    ScriptEntity* self = GET_ENTITY(0);
    ISound* audio = GET_ARG(1, GetSound);
    float panning = GET_ARG_OPT(2, GetDecimal, 0.0f);
    float speed = GET_ARG_OPT(3, GetDecimal, 1.0f);
    float volume = GET_ARG_OPT(4, GetDecimal, 1.0f);
    int channel = -1;
    if (IsValidEntity(self)) {
        AudioManager::StopOriginSound((void*)self, audio);
        channel = AudioManager::PlaySound(audio, false, 0, panning, speed, volume, (void*)self);
    }
    return INTEGER_VAL(channel);
}
/***
 * \method LoopSound
 * \desc Plays a sound from the entity, looping back when it ends.
 * \param sound (Integer): The sound index to play.
 * \paramOpt loopPoint (Integer): Loop point in samples.
 * \paramOpt panning (Decimal): Control the panning of the audio. -1.0 makes it sound in left ear only, 1.0 makes it sound in right ear, and closer to 0.0 centers it. (0.0 is the default.)
 * \paramOpt speed (Decimal): Control the speed of the audio. > 1.0 makes it faster, < 1.0 is slower, 1.0 is normal speed. (1.0 is the default.)
 * \paramOpt volume (Decimal): Controls the volume of the audio. 0.0 is muted, 1.0 is normal volume. (1.0 is the default.)
 * \return Returns the channel index where the sound began to play, or <code>-1</code> if no channel was available.
 * \ns Instance
 */
VMValue ScriptEntity::VM_LoopSound(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckAtLeastArgCount(argCount, 2);
    ScriptEntity* self = GET_ENTITY(0);
    ISound* audio = GET_ARG(1, GetSound);
    int loopPoint = GET_ARG_OPT(2, GetInteger, 0);
    float panning = GET_ARG_OPT(3, GetDecimal, 0.0f);
    float speed = GET_ARG_OPT(4, GetDecimal, 1.0f);
    float volume = GET_ARG_OPT(5, GetDecimal, 1.0f);
    int channel = -1;
    if (IsValidEntity(self)) {
        AudioManager::StopOriginSound((void*)self, audio);
        channel = AudioManager::PlaySound(audio, true, loopPoint, panning, speed, volume, (void*)self);
    }
    return INTEGER_VAL(channel);
}
/***
 * \method StopSound
 * \desc Stops a specific sound that is being played from the entity.
 * \param sound (Integer): The sound index to interrupt.
 * \ns Instance
 */
VMValue ScriptEntity::VM_StopSound(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    ScriptEntity* self = GET_ENTITY(0);
    ISound* audio = GET_ARG(1, GetSound);
    if (IsValidEntity(self))
        AudioManager::StopOriginSound((void*)self, audio);
    return NULL_VAL;
}
/***
 * \method StopAllSounds
 * \desc Stops all sounds the entity is playing.
 * \ns Instance
 */
VMValue ScriptEntity::VM_StopAllSounds(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 1);
    ScriptEntity* self = GET_ENTITY(0);
    if (IsValidEntity(self))
        AudioManager::StopAllOriginSounds((void*)self);
    return NULL_VAL;
}
