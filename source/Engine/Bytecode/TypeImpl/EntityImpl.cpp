#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/ScriptEntity.h>

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

#define ENTITY_NATIVE_FN(name) { \
	ObjNative* native = NewNative(ScriptEntity::VM_##name); \
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

void EntityImpl::Dispose(Obj* object) {
	ObjEntity* entity = (ObjEntity*)object;

	Scene::DeleteRemoved((Entity*)entity->EntityPtr);

	InstanceImpl::Dispose(object);
}
