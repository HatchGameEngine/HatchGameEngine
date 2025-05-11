#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/StringImpl.h>

ObjClass* StringImpl::Class = nullptr;

void StringImpl::Init() {
	Class = NewClass(CLASS_STRING);
	Class->ElementGet = StringImpl::VM_ElementGet;

	ScriptManager::ClassImplList.push_back(Class);
}

void StringImpl::Dispose(Obj* object) {
	ObjString* string = (ObjString*)object;

	Memory::Free(string->Chars);
}

#define THROW_ERROR(...) ScriptManager::Threads[threadID].ThrowRuntimeError(false, __VA_ARGS__)

bool StringImpl::VM_ElementGet(Obj* object, VMValue at, VMValue* result, Uint32 threadID) {
	ObjString* string = (ObjString*)object;

	if (!IS_INTEGER(at)) {
		THROW_ERROR("Cannot get value from array using non-Integer value as an index.");
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
		THROW_ERROR("Index %d is out of bounds of string of length %d.",
			index,
			(int)string->Length);
		if (result) {
			*result = NULL_VAL;
		}
		return true;
	}

	if (result) {
		*result = OBJECT_VAL(CopyString(&string->Chars[index], 1));
	}
	return true;
}
