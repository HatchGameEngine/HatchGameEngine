#if INTERFACE
#include <Engine/Types/Entity.h>
#include <Engine/Bytecode/BytecodeObjectManager.h>

class BytecodeObject : public Entity {
public:
    ObjInstance* Instance = NULL;
    HashMap<VMValue>* Properties;
};
#endif

#include <Engine/Bytecode/BytecodeObject.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Scene.h>

#define LINK_INT(VAR) Instance->Fields->Put(#VAR, INTEGER_LINK_VAL(&VAR))
#define LINK_DEC(VAR) Instance->Fields->Put(#VAR, DECIMAL_LINK_VAL(&VAR))
#define LINK_BOOL(VAR) Instance->Fields->Put(#VAR, INTEGER_LINK_VAL(&VAR))

bool   SavedHashes = false;
Uint32 Hash_GameStart = 0;
Uint32 Hash_Setup = 0;
Uint32 Hash_Create = 0;
Uint32 Hash_Update = 0;
Uint32 Hash_UpdateLate = 0;
Uint32 Hash_UpdateEarly = 0;
Uint32 Hash_RenderEarly = 0;
Uint32 Hash_Render = 0;
Uint32 Hash_RenderLate = 0;
Uint32 Hash_OnAnimationFinish = 0;

PUBLIC void BytecodeObject::Link(ObjInstance* instance) {
    Instance = instance;
    Instance->EntityPtr = this;
    Properties = new HashMap<VMValue>(NULL, 4);

    if (!SavedHashes) {
        Hash_GameStart = Murmur::EncryptString("GameStart");
        Hash_Setup = Murmur::EncryptString("Setup");
        Hash_Create = Murmur::EncryptString("Create");
        Hash_Update = Murmur::EncryptString("Update");
        Hash_UpdateLate = Murmur::EncryptString("UpdateLate");
        Hash_UpdateEarly = Murmur::EncryptString("UpdateEarly");
        Hash_RenderEarly = Murmur::EncryptString("RenderEarly");
        Hash_Render = Murmur::EncryptString("Render");
        Hash_RenderLate = Murmur::EncryptString("RenderLate");
        Hash_OnAnimationFinish = Murmur::EncryptString("OnAnimationFinish");

        SavedHashes = true;
    }

    LINK_DEC(X);
    LINK_DEC(Y);
    LINK_DEC(Z);
    LINK_DEC(XSpeed);
    LINK_DEC(YSpeed);
    LINK_DEC(GroundSpeed);
    LINK_DEC(Gravity);
    LINK_INT(AutoPhysics);
    LINK_INT(Angle);
    LINK_INT(AngleMode);
    LINK_INT(Ground);
    LINK_DEC(Rotation);
    LINK_INT(Priority);
    LINK_DEC(Depth);
    LINK_INT(Sprite);
    LINK_INT(CurrentAnimation);
    LINK_INT(CurrentFrame);
    LINK_INT(CurrentFrameCount);
    LINK_DEC(CurrentFrameTimeLeft);
    LINK_DEC(AnimationSpeedMult);
    LINK_INT(AnimationSpeedAdd);
    LINK_INT(AutoAnimate);

    LINK_INT(OnScreen);
    LINK_DEC(OnScreenHitboxW);
    LINK_DEC(OnScreenHitboxH);
    LINK_INT(ViewRenderFlag);
    LINK_INT(ViewOverrideFlag);

    Instance->Fields->Put("UpdateRegionW", DECIMAL_LINK_VAL(&OnScreenHitboxW));
    Instance->Fields->Put("UpdateRegionH", DECIMAL_LINK_VAL(&OnScreenHitboxH));
    LINK_DEC(RenderRegionW);
    LINK_DEC(RenderRegionH);

    LINK_DEC(HitboxW);
    LINK_DEC(HitboxH);
    LINK_DEC(HitboxOffX);
    LINK_DEC(HitboxOffY);
    LINK_INT(FlipFlag);

    LINK_DEC(SensorX);
    LINK_DEC(SensorY);
    LINK_INT(SensorCollided);
    LINK_INT(SensorAngle);

    LINK_BOOL(Active);
    LINK_BOOL(Pauseable);
    LINK_BOOL(Persistent);
    LINK_BOOL(Interactable);
}

#undef LINK_INT
#undef LINK_DEC
#undef LINK_BOOL

PRIVATE bool BytecodeObject::GetCallableValue(Uint32 hash, VMValue& value) {
    // First look for a field which may shadow a method.
    if (Instance->Fields->Exists(hash)) {
        value = Instance->Fields->Get(hash);
        return true;
    }

    ObjClass* klass = Instance->Class;
    if (klass->Methods->Exists(hash)) {
        value = klass->Methods->Get(hash);
        return true;
    }
    else {
        if (!klass->Parent && klass->ParentHash) {
            BytecodeObjectManager::SetClassParent(klass);
        }
        if (klass->Parent && klass->Parent->Methods->Exists(hash)) {
            value = klass->Parent->Methods->Get(hash);
            return true;
        }
    }

    return false;
}
PRIVATE ObjFunction* BytecodeObject::GetCallableFunction(Uint32 hash) {
    ObjClass* klass = Instance->Class;
    if (klass->Methods->Exists(hash))
        return AS_FUNCTION(klass->Methods->Get(hash));
    return NULL;
}
PUBLIC bool BytecodeObject::RunFunction(Uint32 hash) {
    // NOTE:
    // If the function doesn't exist, this is not an error VM side,
    // treat whatever we call from C++ as a virtual-like function.
    VMValue value;
    if (!BytecodeObject::GetCallableValue(hash, value))
        return true;

    VMThread* thread = BytecodeObjectManager::Threads + 0;

    VMValue* stackTop = thread->StackTop;

    thread->Push(OBJECT_VAL(Instance));
    thread->InvokeForEntity(value, 0);

    thread->StackTop = stackTop;

    return true;
}
PUBLIC bool BytecodeObject::RunCreateFunction(VMValue flag) {
    // NOTE:
    // If the function doesn't exist, this is not an error VM side,
    // treat whatever we call from C++ as a virtual-like function.
    ObjFunction* func = BytecodeObject::GetCallableFunction(Hash_Create);
    if (!func)
        return true;

    VMThread* thread = BytecodeObjectManager::Threads + 0;

    VMValue* stackTop = thread->StackTop;

    if (func->Arity == 1) {
        thread->Push(OBJECT_VAL(Instance));
        thread->Push(flag);
        thread->RunEntityFunction(func, 1);
    }
    else {
        thread->Push(OBJECT_VAL(Instance));
        thread->RunEntityFunction(func, 0);
    }

    thread->StackTop = stackTop;

    return false;
}
PUBLIC bool BytecodeObject::RunInitializer() {
    if (!HasInitializer(Instance->Class))
        return true;

    VMThread* thread = BytecodeObjectManager::Threads + 0;

    VMValue* stackTop = thread->StackTop;

    thread->Push(OBJECT_VAL(Instance)); // Pushes this instance into the stack so that 'this' can work
    thread->CallInitializer(Instance->Class->Initializer);
    thread->Pop(); // Pops it

    thread->StackTop = stackTop;

    return true;
}

// Events called from C++
PUBLIC void BytecodeObject::GameStart() {
    if (!Instance) return;

    RunFunction(Hash_GameStart);
}
PUBLIC void BytecodeObject::Initialize() {
    if (!Instance) return;

    RunInitializer();
}
PUBLIC void BytecodeObject::Create(VMValue flag) {
    if (!Instance) return;

    // Set defaults
    Active = true;
    Pauseable = true;

    XSpeed = 0.0f;
    YSpeed = 0.0f;
    GroundSpeed = 0.0f;
    Gravity = 0.0f;
    Ground = false;

    OnScreen = true;
    OnScreenHitboxW = 0.0f;
    OnScreenHitboxH = 0.0f;
    ViewRenderFlag = 0xFFFFFFFF;
    ViewOverrideFlag = 0;
    RenderRegionW = 0.0f;
    RenderRegionH = 0.0f;

    Angle = 0;
    AngleMode = 0;
    Rotation = 0.0;
    AutoPhysics = false;

    Priority = 0;
    PriorityListIndex = -1;
    PriorityOld = 0;

    Sprite = -1;
    CurrentAnimation = -1;
    CurrentFrame = -1;
    CurrentFrameTimeLeft = 0.0;
    AnimationSpeedMult = 1.0;
    AnimationSpeedAdd = 0;
    AutoAnimate = true;

    HitboxW = 0.0f;
    HitboxH = 0.0f;
    HitboxOffX = 0.0f;
    HitboxOffY = 0.0f;
    FlipFlag = 0;

    Persistent = false;
    Interactable = true;

    RunCreateFunction(flag);
    if (Sprite >= 0 && CurrentAnimation < 0) {
        SetAnimation(0, 0);
    }
}
PUBLIC void BytecodeObject::Create() {
    Create(INTEGER_VAL(0));
}
PUBLIC void BytecodeObject::Setup() {
    if (!Instance) return;

    // RunFunction(Hash_Setup);
}
PUBLIC void BytecodeObject::UpdateEarly() {
    if (!Active) return;
    if (!Instance) return;

    RunFunction(Hash_UpdateEarly);
}
PUBLIC void BytecodeObject::Update() {
    if (!Active) return;
    if (!Instance) return;

    RunFunction(Hash_Update);
}
PUBLIC void BytecodeObject::UpdateLate() {
    if (!Active) return;
    if (!Instance) return;

    RunFunction(Hash_UpdateLate);

    if (AutoAnimate)
        Animate();
    if (AutoPhysics)
        ApplyMotion();
}
PUBLIC void BytecodeObject::RenderEarly() {
    if (!Active) return;
    if (!Instance) return;

    RunFunction(Hash_RenderEarly);
}
PUBLIC void BytecodeObject::Render(int CamX, int CamY) {
    if (!Active) return;
    if (!Instance) return;

    if (RunFunction(Hash_Render)) {
        // Default render
    }
}
PUBLIC void BytecodeObject::RenderLate() {
    if (!Active) return;
    if (!Instance) return;

    RunFunction(Hash_RenderLate);
}
PUBLIC void BytecodeObject::OnAnimationFinish() {
    RunFunction(Hash_OnAnimationFinish);
}

PUBLIC void BytecodeObject::Dispose() {
    Entity::Dispose();
    if (Properties) {
        delete Properties;
    }
    if (Instance) {
        Instance = NULL;
    }
}

// Events/methods called from VM
bool _TestCollision(BytecodeObject* other, BytecodeObject* self) {
    if (!other->Active || other->Removed) return false;
    // if (!other->Instance) return false;
    if (other->HitboxW == 0.0f ||
        other->HitboxH == 0.0f) return false;

    if (other->X + other->HitboxW / 2.0f >= self->X - self->HitboxW / 2.0f &&
        other->Y + other->HitboxH / 2.0f >= self->Y - self->HitboxH / 2.0f &&
        other->X - other->HitboxW / 2.0f  < self->X + self->HitboxW / 2.0f &&
        other->Y - other->HitboxH / 2.0f  < self->Y + self->HitboxH / 2.0f) {
        BytecodeObjectManager::Globals->Put("other", OBJECT_VAL(other->Instance));
        return true;
    }
    return false;
}
PUBLIC STATIC VMValue BytecodeObject::VM_SetAnimation(int argCount, VMValue* args, Uint32 threadID) {
    Entity* self = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    int animation = AS_INTEGER(args[1]);
    int frame = AS_INTEGER(args[2]);

    if (self->Sprite < 0) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "this.Sprite is not set!", animation);
        return NULL_VAL;
    }

    ISprite* sprite = Scene::SpriteList[self->Sprite]->AsSprite;
    if (!(animation >= 0 && (size_t)animation < sprite->Animations.size())) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Animation %d is not in bounds of sprite.", animation);
        return NULL_VAL;
    }
    if (!(frame >= 0 && (size_t)frame < sprite->Animations[animation].Frames.size())) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Frame %d is not in bounds of animation %d.", frame, animation);
        return NULL_VAL;
    }

    self->SetAnimation(animation, frame);
    return NULL_VAL;
}
PUBLIC STATIC VMValue BytecodeObject::VM_ResetAnimation(int argCount, VMValue* args, Uint32 threadID) {
    Entity* self = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    int animation = AS_INTEGER(args[1]);
    int frame = AS_INTEGER(args[2]);

	int spriteIns = self->Sprite;
	if (!(spriteIns > -1 && (size_t)spriteIns < Scene::SpriteList.size())) {
		BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Sprite %d does not exist!", spriteIns);
		return NULL_VAL;
	}

    ISprite* sprite = Scene::SpriteList[self->Sprite]->AsSprite;
    if (!(animation >= 0 && (Uint32)animation < sprite->Animations.size())) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Animation %d is not in bounds of sprite.", animation);
        return NULL_VAL;
    }
    if (!(frame >= 0 && (Uint32)frame < sprite->Animations[animation].Frames.size())) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Frame %d is not in bounds of animation %d.", frame, animation);
        return NULL_VAL;
    }

    self->ResetAnimation(animation, frame);
    return NULL_VAL;
}
PUBLIC STATIC VMValue BytecodeObject::VM_Animate(int argCount, VMValue* args, Uint32 threadID) {
    Entity* self = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    self->Animate();
    return NULL_VAL;
}
PUBLIC STATIC VMValue BytecodeObject::VM_AddToRegistry(int argCount, VMValue* args, Uint32 threadID) {
    Entity* self     = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    char*   registry = AS_CSTRING(args[1]);

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
PUBLIC STATIC VMValue BytecodeObject::VM_RemoveFromRegistry(int argCount, VMValue* args, Uint32 threadID) {
    Entity* self     = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    char*   registry = AS_CSTRING(args[1]);

    ObjectRegistry* objectRegistry;
    if (!Scene::ObjectRegistries->Exists(registry)) {
        return NULL_VAL;
    }
    objectRegistry = Scene::ObjectRegistries->Get(registry);

    objectRegistry->Remove(self);

    return NULL_VAL;
}
PUBLIC STATIC VMValue BytecodeObject::VM_ApplyMotion(int argCount, VMValue* args, Uint32 threadID) {
    Entity* self = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    self->ApplyMotion();
    return NULL_VAL;
}
PUBLIC STATIC VMValue BytecodeObject::VM_InView(int argCount, VMValue* args, Uint32 threadID) {
    // Entity* self = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    int   view   = StandardLibrary::GetInteger(args, 1);
    float x      = StandardLibrary::GetDecimal(args, 2);
    float y      = StandardLibrary::GetDecimal(args, 3);
    float w      = StandardLibrary::GetDecimal(args, 4);
    float h      = StandardLibrary::GetDecimal(args, 5);

    // printf("InView: view %d x %.1f y %.1f w %.1f h %.1f\n        entity x %.1f y %.1f w %.1f h %.1f\n%s\n",
    //     view,
    //     Scene::Views[view].X, Scene::Views[view].Y, Scene::Views[view].Width, Scene::Views[view].Height,
    //     x, y, w, h,
    //     (x + w >= Scene::Views[view].X &&
    //     y + h >= Scene::Views[view].Y &&
    //     x      < Scene::Views[view].X + Scene::Views[view].Width &&
    //     y      < Scene::Views[view].Y + Scene::Views[view].Height) ? "TRUE" : "FALSE");

    if (x + w >= Scene::Views[view].X &&
        y + h >= Scene::Views[view].Y &&
        x      < Scene::Views[view].X + Scene::Views[view].Width &&
        y      < Scene::Views[view].Y + Scene::Views[view].Height)
        return INTEGER_VAL(true);

    return INTEGER_VAL(false);
}
PUBLIC STATIC VMValue BytecodeObject::VM_CollidedWithObject(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);

    if (IS_INSTANCE(args[1])) {
        BytecodeObject* self = (BytecodeObject*)AS_INSTANCE(args[0])->EntityPtr;
        BytecodeObject* other = (BytecodeObject*)AS_INSTANCE(args[1])->EntityPtr;
        return INTEGER_VAL(_TestCollision(other, self));
    }

    if (!Scene::ObjectLists) return INTEGER_VAL(false);
    if (!Scene::ObjectRegistries) return INTEGER_VAL(false);

    char* object = StandardLibrary::GetString(args, 1);
    if (!Scene::ObjectRegistries->Exists(object)) {
        if (!Scene::ObjectLists->Exists(object))
            return INTEGER_VAL(false);
    }

    BytecodeObject* self = (BytecodeObject*)AS_INSTANCE(args[0])->EntityPtr;
    if (self->HitboxW == 0.0f ||
        self->HitboxH == 0.0f) return INTEGER_VAL(false);

    BytecodeObject* other = NULL;
    ObjectList* objectList = Scene::ObjectLists->Get(object);

    other = (BytecodeObject*)objectList->EntityFirst;
    for (Entity* next; other; other = (BytecodeObject*)next) {
        next = other->NextEntityInList;
        if (_TestCollision(other, self))
            return INTEGER_VAL(true);
    }

    return INTEGER_VAL(false);
}
PUBLIC STATIC VMValue BytecodeObject::VM_GetHitboxFromSprite(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 5);
    Entity* self    = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    ISprite* sprite = StandardLibrary::GetSprite(args, 1);
    int animation   = StandardLibrary::GetInteger(args, 2);
    int frame       = StandardLibrary::GetInteger(args, 3);
    int hitbox      = StandardLibrary::GetInteger(args, 4);

    if (!(animation > -1 && (size_t)animation < sprite->Animations.size())) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Animation %d is not in bounds of sprite.", animation);
        return NULL_VAL;
    }
    if (!(frame > -1 && (size_t)frame < sprite->Animations[animation].Frames.size())) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Frame %d is not in bounds of animation %d.", frame, animation);
        return NULL_VAL;
    }

    AnimFrame frameO = sprite->Animations[animation].Frames[frame];

    if (!(hitbox > -1 && hitbox < frameO.BoxCount)) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Hitbox %d is not in bounds of frame %d.", hitbox, frame);
        return NULL_VAL;
    }

    CollisionBox box = frameO.Boxes[hitbox];
    self->HitboxW = box.Right - box.Left;
    self->HitboxH = box.Bottom - box.Top;
    self->HitboxOffX = box.Left + self->HitboxW * 0.5f;
    self->HitboxOffY = box.Top + self->HitboxH * 0.5f;

    return NULL_VAL;
}

/*
bool Entity::CollideWithObject
int  Entity::SolidCollideWithObject
bool Entity::TopSolidCollideWithObject
*/
PUBLIC STATIC VMValue BytecodeObject::VM_CollideWithObject(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    BytecodeObject* self = (BytecodeObject*)AS_INSTANCE(args[0])->EntityPtr;
    BytecodeObject* other = (BytecodeObject*)AS_INSTANCE(args[1])->EntityPtr;
    return INTEGER_VAL(self->CollideWithObject(other));
}
PUBLIC STATIC VMValue BytecodeObject::VM_SolidCollideWithObject(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 3);
    BytecodeObject* self = (BytecodeObject*)AS_INSTANCE(args[0])->EntityPtr;
    BytecodeObject* other = (BytecodeObject*)AS_INSTANCE(args[1])->EntityPtr;
    int flag = StandardLibrary::GetInteger(args, 2);
    return INTEGER_VAL(self->SolidCollideWithObject(other, flag));
}
PUBLIC STATIC VMValue BytecodeObject::VM_TopSolidCollideWithObject(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 3);
    BytecodeObject* self = (BytecodeObject*)AS_INSTANCE(args[0])->EntityPtr;
    BytecodeObject* other = (BytecodeObject*)AS_INSTANCE(args[1])->EntityPtr;
    int flag = StandardLibrary::GetInteger(args, 2);
    return INTEGER_VAL(self->TopSolidCollideWithObject(other, flag));
}

PUBLIC STATIC VMValue BytecodeObject::VM_ApplyPhysics(int argCount, VMValue* args, Uint32 threadID) {
    Entity* self = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    self->ApplyPhysics();
    return NULL_VAL;
}

PUBLIC STATIC VMValue BytecodeObject::VM_PropertyExists(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    BytecodeObject* self = (BytecodeObject*)AS_INSTANCE(args[0])->EntityPtr;
    char* property = AS_CSTRING(args[1]);
    if (self->Properties->Exists(property))
        return INTEGER_VAL(1);
    return INTEGER_VAL(0);
}
PUBLIC STATIC VMValue BytecodeObject::VM_PropertyGet(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);
    BytecodeObject* self = (BytecodeObject*)AS_INSTANCE(args[0])->EntityPtr;
    char* property = AS_CSTRING(args[1]);
    if (!self->Properties->Exists(property))
        return NULL_VAL;
    return self->Properties->Get(property);
}

PUBLIC STATIC VMValue BytecodeObject::VM_SetViewVisibility(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 3);
    BytecodeObject* self = (BytecodeObject*)AS_INSTANCE(args[0])->EntityPtr;
    int viewIndex = AS_INTEGER(args[1]);
    bool visible = AS_INTEGER(args[2]);
    int flag = 1 << viewIndex;
    if (visible)
        self->ViewRenderFlag |= flag;
    else
        self->ViewRenderFlag &= ~flag;
    return NULL_VAL;
}
PUBLIC STATIC VMValue BytecodeObject::VM_SetViewOverride(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 3);
    BytecodeObject* self = (BytecodeObject*)AS_INSTANCE(args[0])->EntityPtr;
    int viewIndex = AS_INTEGER(args[1]);
    bool override = AS_INTEGER(args[2]);
    int flag = 1 << viewIndex;
    if (override)
        self->ViewOverrideFlag |= flag;
    else
        self->ViewOverrideFlag &= ~flag;
    return NULL_VAL;
}
