#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Types.h>

ObjClass* EntityImpl::Class = nullptr;
ObjClass* EntityImpl::ParentClass = nullptr;

#define ENTITY_CLASS_NAME "Entity"
#define NATIVEENTITY_CLASS_NAME "NativeEntity"

Uint32 Hash_HitboxLeft = 0;
Uint32 Hash_HitboxTop = 0;
Uint32 Hash_HitboxRight = 0;
Uint32 Hash_HitboxBottom = 0;

void EntityImpl::Init() {
	Class = NewClass(ENTITY_CLASS_NAME);
	ParentClass = NewClass(NATIVEENTITY_CLASS_NAME);

	Class->Parent = ParentClass;

#define ENTITY_NATIVE_FN(name) \
	{ \
		ObjNative* native = NewNative(EntityImpl::VM_##name); \
		Class->Methods->Put(#name, OBJECT_VAL(native)); \
		ParentClass->Methods->Put(#name, OBJECT_VAL(native)); \
	}
	ENTITY_NATIVE_FN_LIST
#undef ENTITY_NATIVE_FN

	Hash_HitboxLeft = Murmur::EncryptString("HitboxLeft");
	Hash_HitboxTop = Murmur::EncryptString("HitboxTop");
	Hash_HitboxRight = Murmur::EncryptString("HitboxRight");
	Hash_HitboxBottom = Murmur::EncryptString("HitboxBottom");

	TypeImpl::RegisterClass(Class);
	TypeImpl::RegisterClass(ParentClass);

	// NativeEntity is not put into Globals so that you can't do anything with it.
	ScriptManager::Globals->Put(Class->Hash, OBJECT_VAL(Class));
}

Obj* EntityImpl::New(ObjClass* klass) {
	ObjEntity* entity = (ObjEntity*)InstanceImpl::New(sizeof(ObjEntity), OBJ_ENTITY);
	Memory::Track(entity, "NewEntity");
	entity->Object.Class = klass;
	entity->Object.PropertyGet = VM_PropertyGet;
	entity->Object.PropertySet = VM_PropertySet;
	entity->Object.Destructor = Dispose;
	return (Obj*)entity;
}

bool EntityImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	ObjEntity* objEntity = (ObjEntity*)object;
	Entity* entity = (Entity*)objEntity->EntityPtr;

	if (hash == Hash_HitboxLeft) {
		if (result) {
			*result = DECIMAL_VAL(entity->Hitbox.GetLeft());
		}
		return true;
	}
	else if (hash == Hash_HitboxTop) {
		if (result) {
			*result = DECIMAL_VAL(entity->Hitbox.GetTop());
		}
		return true;
	}
	else if (hash == Hash_HitboxRight) {
		if (result) {
			*result = DECIMAL_VAL(entity->Hitbox.GetRight());
		}
		return true;
	}
	else if (hash == Hash_HitboxBottom) {
		if (result) {
			*result = DECIMAL_VAL(entity->Hitbox.GetBottom());
		}
		return true;
	}

	return false;
}
bool EntityImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	ObjEntity* objEntity = (ObjEntity*)object;
	Entity* entity = (Entity*)objEntity->EntityPtr;

	if (hash == Hash_HitboxLeft) {
		if (ScriptManager::DoDecimalConversion(value, threadID)) {
			entity->Hitbox.SetLeft(AS_DECIMAL(value));
		}
		return true;
	}
	else if (hash == Hash_HitboxTop) {
		if (ScriptManager::DoDecimalConversion(value, threadID)) {
			entity->Hitbox.SetTop(AS_DECIMAL(value));
		}
		return true;
	}
	else if (hash == Hash_HitboxRight) {
		if (ScriptManager::DoDecimalConversion(value, threadID)) {
			entity->Hitbox.SetRight(AS_DECIMAL(value));
		}
		return true;
	}
	else if (hash == Hash_HitboxBottom) {
		if (ScriptManager::DoDecimalConversion(value, threadID)) {
			entity->Hitbox.SetBottom(AS_DECIMAL(value));
		}
		return true;
	}

	return false;
}

// Events/methods called from VM
#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, StandardLibrary::argFunction) : argDefault)
#define GET_ENTITY(argIndex) (GetScriptEntity(args, argIndex, threadID))
ScriptEntity* GetScriptEntity(VMValue* args, int index, Uint32 threadID) {
	ObjEntity* entity = GET_ARG(index, GetEntity);
	if (entity) {
		ScriptEntity* self = (ScriptEntity*)entity->EntityPtr;
		if (self && self->IsValid()) {
			return self;
		}
	}

	return nullptr;
}
/***
 * \method SetAnimation
 * \desc Changes the current animation of the entity, if the animation index differs from the entity's current animation index.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index.
 * \ns Entity
 */
VMValue EntityImpl::VM_SetAnimation(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);
	ScriptEntity* self = GET_ENTITY(0);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);

	if (!self) {
		return NULL_VAL;
	}

	ResourceType* resource = Scene::GetSpriteResource(self->Sprite);
	if (!resource) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Sprite is not set!", animation);
		return NULL_VAL;
	}

	ISprite* sprite = resource->AsSprite;
	if (!sprite) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Sprite is not set!", animation);
		return NULL_VAL;
	}

	if (!(animation >= 0 && (size_t)animation < sprite->Animations.size())) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Animation %d is not in bounds of sprite.", animation);
		return NULL_VAL;
	}
	if (!(frame >= 0 && (size_t)frame < sprite->Animations[animation].Frames.size())) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Frame %d is not in bounds of animation %d.", frame, animation);
		return NULL_VAL;
	}

	self->Entity::SetAnimation(animation, frame);
	return NULL_VAL;
}
/***
 * \method ResetAnimation
 * \desc Changes the current animation of the entity.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index.
 * \ns Entity
 */
VMValue EntityImpl::VM_ResetAnimation(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);
	ScriptEntity* self = GET_ENTITY(0);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);

	if (!self) {
		return NULL_VAL;
	}

	ResourceType* resource = Scene::GetSpriteResource(self->Sprite);
	if (!resource) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Sprite %d does not exist!", self->Sprite);
		return NULL_VAL;
	}

	ISprite* sprite = resource->AsSprite;
	if (!sprite) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Sprite %d does not exist!", self->Sprite);
		return NULL_VAL;
	}

	if (!(animation >= 0 && (Uint32)animation < sprite->Animations.size())) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Animation %d is not in bounds of sprite.", animation);
		return NULL_VAL;
	}
	if (!(frame >= 0 && (Uint32)frame < sprite->Animations[animation].Frames.size())) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Frame %d is not in bounds of animation %d.", frame, animation);
		return NULL_VAL;
	}

	self->Entity::ResetAnimation(animation, frame);
	return NULL_VAL;
}
/***
 * \method Animate
 * \desc Animates the entity.
 * \ns Entity
 */
VMValue EntityImpl::VM_Animate(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);
	ScriptEntity* self = GET_ENTITY(0);
	if (self) {
		self->Entity::Animate();
	}
	return NULL_VAL;
}

/***
 * \method GetUpdatePriority
 * \desc Gets the update priority of the entity.
 * \return Returns an Integer value.
 * \ns Entity
 */
VMValue EntityImpl::VM_GetUpdatePriority(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);
	ScriptEntity* self = GET_ENTITY(0);
	if (self) {
		return INTEGER_VAL(self->UpdatePriority);
	}
	return NULL_VAL;
}
/***
 * \method SetUpdatePriority
 * \desc Sets the update priority of the entity. Higher numbers cause entities to be updated sooner, and lower numbers cause entities to be updated later. If multiple entities have the same update priority, they are sorted by spawn order; ascending for positive priority values, and descending for negative priority values.
 * \param priority (Integer): The priority value.
 * \ns Entity
 */
VMValue EntityImpl::VM_SetUpdatePriority(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	int priority = GET_ARG(1, GetInteger);

	if (self) {
		self->SetUpdatePriority(priority);
	}

	return NULL_VAL;
}

/***
 * \method GetIDWithinClass
 * \desc Gets the ordered ID of the entity amongst other entities of the same type.
 * \return Returns an Integer value.
 * \ns Entity
 */
VMValue EntityImpl::VM_GetIDWithinClass(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);
	ScriptEntity* self = GET_ENTITY(0);
	if (!self) {
		return NULL_VAL;
	}

	return INTEGER_VAL(self->GetIDWithinClass());
}

/***
 * \method AddToRegistry
 * \desc Adds the entity to a registry.
 * \param registry (String): The registry name.
 * \ns Entity
 */
VMValue EntityImpl::VM_AddToRegistry(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	char* registry = GET_ARG(1, GetString);

	if (!self) {
		return NULL_VAL;
	}

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
 * \ns Entity
 */
VMValue EntityImpl::VM_IsInRegistry(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	char* registry = GET_ARG(1, GetString);

	if (!self || !Scene::ObjectRegistries->Exists(registry)) {
		return NULL_VAL;
	}

	ObjectRegistry* objectRegistry = Scene::ObjectRegistries->Get(registry);

	return INTEGER_VAL(objectRegistry->Contains(self));
}
/***
 * \method RemoveFromRegistry
 * \desc Removes the entity from a registry.
 * \param registry (String): The registry name.
 * \ns Entity
 */
VMValue EntityImpl::VM_RemoveFromRegistry(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	char* registry = GET_ARG(1, GetString);

	if (!self || !Scene::ObjectRegistries->Exists(registry)) {
		return NULL_VAL;
	}

	ObjectRegistry* objectRegistry = Scene::ObjectRegistries->Get(registry);

	objectRegistry->Remove(self);

	return NULL_VAL;
}
/***
 * \method ApplyMotion
 * \desc Applies gravity and velocities to the entity.
 * \ns Entity
 */
VMValue EntityImpl::VM_ApplyMotion(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);
	ScriptEntity* self = GET_ENTITY(0);
	if (self) {
		self->Entity::ApplyMotion();
	}
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
 * \ns Entity
 */
VMValue EntityImpl::VM_InView(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 6);
	// Entity* self = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
	int view = GET_ARG(1, GetInteger);
	float x = GET_ARG(2, GetDecimal);
	float y = GET_ARG(3, GetDecimal);
	float w = GET_ARG(4, GetDecimal);
	float h = GET_ARG(5, GetDecimal);

	if (x + w >= Scene::Views[view].X && y + h >= Scene::Views[view].Y &&
		x < Scene::Views[view].X + Scene::Views[view].Width &&
		y < Scene::Views[view].Y + Scene::Views[view].Height) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * \method CollidedWithObject
 * \desc Checks if the entity collided with another entity, or any entity of the specified class name.
 * \param other (Instance/String): The entity or class to collide with.
 * \return Returns the entity that was collided with, or <code>null</code> if it did not collide with any entity.
 * \ns Entity
 */
VMValue EntityImpl::VM_CollidedWithObject(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ScriptEntity* self = GET_ENTITY(0);
	if (!self) {
		return NULL_VAL;
	}

	if (IS_ENTITY(args[1])) {
		ScriptEntity* other = GET_ENTITY(1);
		if (other && self->CollideWithObject(other)) {
			return OBJECT_VAL(other->Instance);
		}
		return NULL_VAL;
	}

	if (!Scene::ObjectLists) {
		return NULL_VAL;
	}
	if (!Scene::ObjectRegistries) {
		return NULL_VAL;
	}

	char* object = GET_ARG(1, GetString);
	if (!Scene::ObjectRegistries->Exists(object)) {
		if (!Scene::ObjectLists->Exists(object)) {
			return NULL_VAL;
		}
	}

	if (self->Hitbox.Width == 0.0f || self->Hitbox.Height == 0.0f) {
		return NULL_VAL;
	}

	ScriptEntity* other = NULL;
	ObjectList* objectList = Scene::ObjectLists->Get(object);

	other = (ScriptEntity*)objectList->EntityFirst;
	for (Entity* next; other; other = (ScriptEntity*)next) {
		next = other->NextEntityInList;
		if (other && self->CollideWithObject(other)) {
			return OBJECT_VAL(other->Instance);
		}
	}

	return NULL_VAL;
}
/***
 * \method GetHitboxFromSprite
 * \desc Updates the entity's hitbox with the hitbox in the specified sprite's animation, frame and hitbox ID or name.
 * \param sprite (Sprite): The sprite.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index.
 * \paramOpt hitbox (String or Integer): The hitbox name or index. Defaults to <code>0</code>.
 * \ns Entity
 */
VMValue EntityImpl::VM_GetHitboxFromSprite(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 4);
	ScriptEntity* self = GET_ENTITY(0);
	ISprite* sprite = GET_ARG(1, GetSprite);
	int animationID = GET_ARG(2, GetInteger);
	int frameID = GET_ARG(3, GetInteger);
	int hitboxID = 0;

	if (!self || !sprite) {
		return NULL_VAL;
	}

	if (!(animationID > -1 && (size_t)animationID < sprite->Animations.size())) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Animation %d is not in bounds of sprite.", animationID);
		return NULL_VAL;
	}
	if (!(frameID > -1 && (size_t)frameID < sprite->Animations[animationID].Frames.size())) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Frame %d is not in bounds of animation %d.", frameID, animationID);
		return NULL_VAL;
	}

	AnimFrame frame = sprite->Animations[animationID].Frames[frameID];

	if (argCount >= 4 && IS_STRING(args[4])) {
		char* name = GET_ARG(4, GetString);
		if (name) {
			int boxIndex = -1;

			for (size_t i = 0; i < frame.Boxes.size(); i++) {
				if (strcmp(frame.Boxes[i].Name.c_str(), name) == 0) {
					boxIndex = (int)i;
					break;
				}
			}

			if (boxIndex != -1) {
				hitboxID = boxIndex;
			}
			else {
				ScriptManager::Threads[threadID].ThrowRuntimeError(false,
					"No hitbox named \"%s\" in frame %d of animation %d.",
					name,
					frameID,
					animationID);
			}
		}
	}
	else {
		hitboxID = GET_ARG_OPT(4, GetInteger, 0);
	}

	if (hitboxID >= 0 && hitboxID < (int)frame.Boxes.size()) {
		self->Hitbox.Set(frame.Boxes[hitboxID]);
	}
	else {
		self->Hitbox.Clear();
	}

	return NULL_VAL;
}
/***
 * \method ReturnHitbox
 * \desc Gets the hitbox of a sprite frame.
 * \param sprite (Integer): The sprite index to check.
 * \param animationID (Integer): The animation index of the sprite to check.
 * \param frameID (Integer): The frame index of the animation to check.
 * \paramOpt hitbox (String or Integer): The hitbox name or index. Defaults to <code>0</code>.
 * \return Returns an Array value.
 * \ns Entity
 */
VMValue EntityImpl::VM_ReturnHitbox(int argCount, VMValue* args, Uint32 threadID) {
	ScriptEntity* self = GET_ENTITY(0);
	if (!self) {
		return NULL_VAL;
	}

	ISprite* sprite;
	int animationID = 0, frameID = 0, hitboxID = 0;
	int hitboxArgNum;

	if (argCount <= 2) {
		if (self->Sprite < 0 || self->Sprite >= (int)Scene::SpriteList.size()) {
			ScriptManager::Threads[threadID].ThrowRuntimeError(
				false, "Sprite index \"%d\" outside bounds of list.", self->Sprite);
			return NULL_VAL;
		}

		if (!Scene::SpriteList[self->Sprite]) {
			ScriptManager::Threads[threadID].ThrowRuntimeError(
				false, "Sprite %d does not exist!", self->Sprite);
			return NULL_VAL;
		}

		sprite = Scene::SpriteList[self->Sprite]->AsSprite;
		animationID = self->CurrentAnimation;
		frameID = self->CurrentFrame;
		hitboxArgNum = 1;
	}
	else {
		StandardLibrary::CheckAtLeastArgCount(argCount, 4);
		sprite = GET_ARG(1, GetSprite);
		animationID = GET_ARG(2, GetInteger);
		frameID = GET_ARG(3, GetInteger);
		hitboxArgNum = 4;
	}

	if (!sprite) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Sprite %d does not exist!", self->Sprite);
		return NULL_VAL;
	}

	if (!(animationID >= 0 && (Uint32)animationID < sprite->Animations.size())) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Animation %d is not in bounds of sprite.", animationID);
		return NULL_VAL;
	}
	if (!(frameID >= 0 && (Uint32)frameID < sprite->Animations[animationID].Frames.size())) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Frame %d is not in bounds of animation %d.", frameID, animationID);
		return NULL_VAL;
	}

	AnimFrame frame = sprite->Animations[animationID].Frames[frameID];

	if (argCount > hitboxArgNum && IS_STRING(args[hitboxArgNum])) {
		char* name = GET_ARG(hitboxArgNum, GetString);
		if (name) {
			int boxIndex = -1;

			for (size_t i = 0; i < frame.Boxes.size(); i++) {
				if (strcmp(frame.Boxes[i].Name.c_str(), name) == 0) {
					boxIndex = (int)i;
					break;
				}
			}

			if (boxIndex != -1) {
				hitboxID = boxIndex;
			}
			else {
				ScriptManager::Threads[threadID].ThrowRuntimeError(false,
					"No hitbox named \"%s\" in frame %d of animation %d.",
					name,
					frameID,
					animationID);
			}
		}
	}
	else {
		hitboxID = GET_ARG_OPT(hitboxArgNum, GetInteger, 0);
	}

	if (frame.Boxes.size() == 0) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false,
			"Frame %d of animation %d contains no hitboxes.",
			frameID,
			animationID);
		return NULL_VAL;
	}
	else if (!(hitboxID > -1 && hitboxID < frame.Boxes.size())) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false,
			"Hitbox %d is not in bounds of frame %d of animation %d.",
			hitboxID,
			frameID,
			animationID);
		return NULL_VAL;
	}

	CollisionBox box = frame.Boxes[hitboxID];
	ObjArray* hitbox = NewArray();
	hitbox->Values->push_back(INTEGER_VAL(box.Left));
	hitbox->Values->push_back(INTEGER_VAL(box.Top));
	hitbox->Values->push_back(INTEGER_VAL(box.Right));
	hitbox->Values->push_back(INTEGER_VAL(box.Bottom));
	return OBJECT_VAL(hitbox);
}

/***
 * \method CollideWithObject
 * \desc Does collision with another entity.
 * \param other (Instance): The other entity to check collision for.
 * \return Returns <code>true</code> if the entity collided, <code>false</code> if otherwise.
 * \ns Entity
 */
VMValue EntityImpl::VM_CollideWithObject(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	ScriptEntity* other = GET_ENTITY(1);
	if (self && other) {
		return INTEGER_VAL(self->Entity::CollideWithObject(other));
	}
	return NULL_VAL;
}
/***
 * \method SolidCollideWithObject
 * \desc Does solid collision with another entity.
 * \param other (Instance): The other entity to check collision for.
 * \return Returns <code>true</code> if the entity collided, <code>false</code> if otherwise.
 * \ns Entity
 */
VMValue EntityImpl::VM_SolidCollideWithObject(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);
	ScriptEntity* self = GET_ENTITY(0);
	ScriptEntity* other = GET_ENTITY(1);
	int flag = GET_ARG(2, GetInteger);
	if (self && other) {
		return INTEGER_VAL(self->Entity::SolidCollideWithObject(other, flag));
	}
	return NULL_VAL;
}
/***
 * \method TopSolidCollideWithObject
 * \desc Does solid collision with another entity's top.
 * \param other (Instance): The other entity to check collision for.
 * \return Returns <code>true</code> if the entity collided, <code>false</code> if otherwise.
 * \ns Entity
 */
VMValue EntityImpl::VM_TopSolidCollideWithObject(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);
	ScriptEntity* self = GET_ENTITY(0);
	ScriptEntity* other = GET_ENTITY(1);
	int flag = GET_ARG(2, GetInteger);
	if (self && other) {
		return INTEGER_VAL(self->Entity::TopSolidCollideWithObject(other, flag));
	}
	return NULL_VAL;
}

VMValue EntityImpl::VM_ApplyPhysics(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);
	ScriptEntity* self = GET_ENTITY(0);
	if (self) {
		self->ApplyPhysics();
	}
	return NULL_VAL;
}

/***
 * \method PropertyExists
 * \desc Checks if a property exists in the entity.
 * \param property (String): The property name.
 * \return Returns <code>true</code> if the property exists, <code>false</code> if otherwise.
 * \ns Entity
 */
VMValue EntityImpl::VM_PropertyExists(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	char* property = GET_ARG(1, GetString);
	if (self && self->Properties->Exists(property)) {
		return INTEGER_VAL(1);
	}
	return INTEGER_VAL(0);
}
/***
 * \method PropertyGet
 * \desc Gets a property exists from the entity.
 * \param property (String): The property name.
 * \return Returns the property if it exists, and <code>null</code> if the property does not exist.
 * \ns Entity
 */
VMValue EntityImpl::VM_PropertyGet(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	char* property = GET_ARG(1, GetString);
	if (self && self->Properties->Exists(property)) {
		return self->Properties->Get(property);
	}
	return NULL_VAL;
}

/***
 * \method SetViewVisibility
 * \desc Sets whether the entity is visible on a specific view.
 * \param viewIndex (Integer): The view index.
 * \param visible (Boolean): Whether the entity will be visible or not on the specified view.
 * \ns Entity
 */
VMValue EntityImpl::VM_SetViewVisibility(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);
	ScriptEntity* self = GET_ENTITY(0);
	int viewIndex = GET_ARG(1, GetInteger);
	bool visible = GET_ARG(2, GetInteger);
	if (self) {
		int flag = 1 << viewIndex;
		if (visible) {
			self->ViewRenderFlag |= flag;
		}
		else {
			self->ViewRenderFlag &= ~flag;
		}
	}
	return NULL_VAL;
}
/***
 * \method SetViewOverride
 * \desc Toggles the bypass for each view's entity rendering toggle set by <linkto ref="Scene.SetObjectViewRender"></linkto>.
 * \param viewIndex (Integer): The view index.
 * \param visible (Boolean): Whether the entity will always be visible or not on the specified view.
 * \ns Entity
 */
VMValue EntityImpl::VM_SetViewOverride(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);
	ScriptEntity* self = GET_ENTITY(0);
	int viewIndex = GET_ARG(1, GetInteger);
	bool override = GET_ARG(2, GetInteger);
	if (self) {
		int flag = 1 << viewIndex;
		if (override) {
			self->ViewOverrideFlag |= flag;
		}
		else {
			self->ViewOverrideFlag &= ~flag;
		}
	}
	return NULL_VAL;
}

/***
 * \method AddToDrawGroup
 * \desc Adds the entity into the specified draw group.
 * \param drawGroup (Integer): The draw group.
 * \ns Entity
 */
VMValue EntityImpl::VM_AddToDrawGroup(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	if (!self) {
		return NULL_VAL;
	}
	int drawGroup = GET_ARG(1, GetInteger);
	if (drawGroup < 0 || drawGroup >= MAX_PRIORITY_PER_LAYER) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false,
			"Draw group %d out of range. (0 - %d)",
			drawGroup,
			MAX_PRIORITY_PER_LAYER - 1);
		return NULL_VAL;
	}
	DrawGroupList* drawGroupList = Scene::GetDrawGroup(drawGroup);
	if (!drawGroupList->Contains(self)) {
		drawGroupList->Add(self);
	}
	return NULL_VAL;
}
/***
 * \method IsInDrawGroup
 * \desc Checks if the entity is in the specified draw group.
 * \param drawGroup (Integer): The draw group.
 * \return Returns <code>true</code> if the entity is in the specified draw group, <code>false</code> if otherwise.
 * \ns Entity
 */
VMValue EntityImpl::VM_IsInDrawGroup(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	if (!self) {
		return INTEGER_VAL(false);
	}
	int drawGroup = GET_ARG(1, GetInteger);
	if (drawGroup < 0 || drawGroup >= MAX_PRIORITY_PER_LAYER) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false,
			"Draw group %d out of range. (0 - %d)",
			drawGroup,
			MAX_PRIORITY_PER_LAYER - 1);
		return NULL_VAL;
	}
	DrawGroupList* drawGroupList = Scene::GetDrawGroupNoCheck(drawGroup);
	if (drawGroupList) {
		return INTEGER_VAL(drawGroupList->Contains(self));
	}
	return INTEGER_VAL(0);
}
/***
 * \method RemoveFromDrawGroup
 * \desc Removes the entity from the specified draw group.
 * \param drawGroup (Integer): The draw group.
 * \ns Entity
 */
VMValue EntityImpl::VM_RemoveFromDrawGroup(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	if (!self) {
		return NULL_VAL;
	}
	int drawGroup = GET_ARG(1, GetInteger);
	if (drawGroup < 0 || drawGroup >= MAX_PRIORITY_PER_LAYER) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false,
			"Draw group %d out of range. (0 - %d)",
			drawGroup,
			MAX_PRIORITY_PER_LAYER - 1);
		return NULL_VAL;
	}
	DrawGroupList* drawGroupList = Scene::GetDrawGroupNoCheck(drawGroup);
	if (drawGroupList) {
		drawGroupList->Remove(self);
	}
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
 * \ns Entity
 */
VMValue EntityImpl::VM_PlaySound(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	ISound* audio = GET_ARG(1, GetSound);
	float panning = GET_ARG_OPT(2, GetDecimal, 0.0f);
	float speed = GET_ARG_OPT(3, GetDecimal, 1.0f);
	float volume = GET_ARG_OPT(4, GetDecimal, 1.0f);
	int channel = -1;
	if (self) {
		AudioManager::StopOriginSound((void*)self, audio);
		channel = AudioManager::PlaySound(
			audio, false, 0, panning, speed, volume, (void*)self);
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
 * \ns Entity
 */
VMValue EntityImpl::VM_LoopSound(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	ISound* audio = GET_ARG(1, GetSound);
	int loopPoint = GET_ARG_OPT(2, GetInteger, 0);
	float panning = GET_ARG_OPT(3, GetDecimal, 0.0f);
	float speed = GET_ARG_OPT(4, GetDecimal, 1.0f);
	float volume = GET_ARG_OPT(5, GetDecimal, 1.0f);
	int channel = -1;
	if (self) {
		AudioManager::StopOriginSound((void*)self, audio);
		channel = AudioManager::PlaySound(
			audio, true, loopPoint, panning, speed, volume, (void*)self);
	}
	return INTEGER_VAL(channel);
}
/***
 * \method StopSound
 * \desc Stops a specific sound that is being played from the entity.
 * \param sound (Integer): The sound index to interrupt.
 * \ns Entity
 */
VMValue EntityImpl::VM_StopSound(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);
	ScriptEntity* self = GET_ENTITY(0);
	ISound* audio = GET_ARG(1, GetSound);
	if (self) {
		AudioManager::StopOriginSound((void*)self, audio);
	}
	return NULL_VAL;
}
/***
 * \method StopAllSounds
 * \desc Stops all sounds the entity is playing.
 * \ns Entity
 */
VMValue EntityImpl::VM_StopAllSounds(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);
	ScriptEntity* self = GET_ENTITY(0);
	if (self) {
		AudioManager::StopAllOriginSounds((void*)self);
	}
	return NULL_VAL;
}

void EntityImpl::Dispose(Obj* object) {
	ObjEntity* entity = (ObjEntity*)object;

	Scene::DeleteRemoved((Entity*)entity->EntityPtr);

	InstanceImpl::Dispose(object);
}
