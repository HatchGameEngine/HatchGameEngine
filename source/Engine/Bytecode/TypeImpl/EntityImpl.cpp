#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/ScriptEntity.h>

ObjClass* EntityImpl::Class = nullptr;

void EntityImpl::Init() {
	const char* name = "Entity";

	Class = NewClass(Murmur::EncryptString(name));
	Class->Name = CopyString(name);

#define ENTITY_NATIVE_FN(name) ScriptManager::DefineNative(Class, #name, ScriptEntity::VM_##name);
	ENTITY_NATIVE_FN_LIST
#undef ENTITY_NATIVE_FN

	ScriptManager::ClassImplList.push_back(Class);

	ScriptManager::Globals->Put(Class->Hash, OBJECT_VAL(Class));
}
