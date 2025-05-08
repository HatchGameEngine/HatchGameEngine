#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/ScriptEntity.h>

ObjClass* EntityImpl::Class = nullptr;
Table* EntityImpl::NativeMethods = nullptr;

void EntityImpl::Init() {
	const char* name = "Entity";

	Class = NewClass(Murmur::EncryptString(name));
	Class->Name = CopyString(name);

	NativeMethods = new Table(NULL, 32);

#define ENTITY_NATIVE_FN(name) { \
	ObjNative* native = NewNative(ScriptEntity::VM_##name); \
	Class->Methods->Put(#name, OBJECT_VAL(native)); \
	NativeMethods->Put(#name, OBJECT_VAL(native)); \
}
	ENTITY_NATIVE_FN_LIST
#undef ENTITY_NATIVE_FN

	ScriptManager::ClassImplList.push_back(Class);

	ScriptManager::Globals->Put(Class->Hash, OBJECT_VAL(Class));
}
