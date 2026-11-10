#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/Value.h>

Obj* InstanceImpl::New(size_t size, ObjType type) {
	ObjInstance* instance = (ObjInstance*)AllocateObject(size, type);
	Memory::Track(instance, "NewInstance");
	instance->Fields = new Table(NULL, 16);
	instance->Destructor = Dispose;
	return (Obj*)instance;
}

void InstanceImpl::Dispose(Obj* object) {
	ObjInstance* instance = (ObjInstance*)object;

	// An instance does not own its values, so it's not allowed to free them.
	delete instance->Fields;
}

void InstanceImpl::CopyFields(ObjInstance* dest, ObjInstance* src) {
	Table* destFields = dest->Fields;
	Table* srcFields = src->Fields;

	destFields->Clear();

	srcFields->WithAll([destFields](Uint32 key, VMValue value) -> void {
		destFields->Put(key, Value::Delink(value));
	});
}
