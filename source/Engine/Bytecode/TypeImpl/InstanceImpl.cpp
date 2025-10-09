#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* InstanceImpl::Class = nullptr;

#define CLASS_INSTANCE "$$InstanceImpl"

void InstanceImpl::Init() {
	Class = NewClass(CLASS_INSTANCE);

	TypeImpl::RegisterClass(Class);
}

Obj* InstanceImpl::New(size_t size, ObjType type) {
	ObjInstance* instance = (ObjInstance*)AllocateObject(size, type);
	Memory::Track(instance, "NewInstance");
	instance->Fields = new Table(NULL, 16);
	instance->Object.Destructor = Dispose;
	return (Obj*)instance;
}

void InstanceImpl::Dispose(Obj* object) {
	ObjInstance* instance = (ObjInstance*)object;

	// An instance does not own its values, so it's not allowed to free them.
	delete instance->Fields;
}
