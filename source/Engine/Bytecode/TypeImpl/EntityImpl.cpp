#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/ScriptEntity.h>

ObjClass* EntityImpl::Class = nullptr;
ObjClass* EntityImpl::ParentClass = nullptr;

#define ENTITY_CLASS_NAME "Entity"
#define NATIVEENTITY_CLASS_NAME "NativeEntity"

void EntityImpl::Init() {
	Class = NewClass(Murmur::EncryptString(ENTITY_CLASS_NAME));
	Class->Name = CopyString(ENTITY_CLASS_NAME);

	ParentClass = NewClass(Murmur::EncryptString(NATIVEENTITY_CLASS_NAME));
	ParentClass->Name = CopyString(NATIVEENTITY_CLASS_NAME);
	Class->ParentHash = ParentClass->Hash;
	Class->Parent = ParentClass;

#define ENTITY_NATIVE_FN(name) { \
	ObjNative* native = NewNative(ScriptEntity::VM_##name); \
	Class->Methods->Put(#name, OBJECT_VAL(native)); \
	ParentClass->Methods->Put(#name, OBJECT_VAL(native)); \
}
	ENTITY_NATIVE_FN_LIST
#undef ENTITY_NATIVE_FN

	ScriptManager::ClassImplList.push_back(Class);
	ScriptManager::ClassImplList.push_back(ParentClass);

	ScriptManager::Globals->Put(Class->Hash, OBJECT_VAL(Class));

	// NativeEntity is not put into Globals so that you can't do anything with it.
}
