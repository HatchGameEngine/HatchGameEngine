#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>

ObjClass* InstanceImpl::Class = nullptr;

void InstanceImpl::Init() {
	Class = NewClass(CLASS_INSTANCE);

	ScriptManager::ClassImplList.push_back(Class);
}

void InstanceImpl::Dispose(Obj* object) {
	ObjInstance* instance = (ObjInstance*)object;

	// An instance does not own its values, so it's not allowed to free them.
	delete instance->Fields;
}
