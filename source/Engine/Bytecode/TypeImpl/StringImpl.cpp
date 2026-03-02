#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/StringImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

StringImpl::StringImpl(ScriptManager* manager) {
	Manager = manager;
	Class = Manager->NewClass(CLASS_STRING);
#ifdef HSL_VM
	Class->ElementGet = VM_ElementGet;
#endif

	TypeImpl::RegisterClass(manager, Class);
}

Obj* StringImpl::New(char* chars, size_t length) {
	ObjString* string = (ObjString*)Manager->AllocateObject(sizeof(ObjString), OBJ_STRING);
	Memory::Track(string, "NewString");
	string->Object.Class = Class;
	string->Length = length;
	string->Chars = chars;
	return (Obj*)string;
}

void StringImpl::Dispose(Obj* object) {
	ObjString* string = (ObjString*)object;

	Memory::Free(string->Chars);
}

#ifdef HSL_VM
bool StringImpl::VM_ElementGet(Obj* object, VMValue at, VMValue* result, VMThread* thread) {
	ObjString* string = (ObjString*)object;

	if (!IS_INTEGER(at)) {
		VM_THROW_ERROR("Cannot get value from array using non-Integer value as an index.");
		if (result) {
			*result = NULL_VAL;
		}
		return true;
	}

	int index = AS_INTEGER(at);
	if (index < 0) {
		index = string->Length + index;
	}

	if (index >= string->Length) {
		VM_THROW_ERROR("Index %d is out of bounds of string of length %d.",
			index,
			(int)string->Length);
		if (result) {
			*result = NULL_VAL;
		}
		return true;
	}

	if (result) {
		*result = OBJECT_VAL(thread->Manager->CopyString(&string->Chars[index], 1));
	}
	return true;
}
#endif
