#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/StringImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* StringImpl::Class = nullptr;

void StringImpl::Init() {
	Class = NewClass(CLASS_STRING);

	TypeImpl::RegisterClass(Class);
}

Obj* StringImpl::New(char* chars, size_t length, Uint32 hash) {
	ObjString* string = (ObjString*)AllocateObject(sizeof(ObjString), OBJ_STRING);
	Memory::Track(string, "NewString");
	string->Object.Class = Class;
	string->Object.ElementGet = VM_ElementGet;
	string->Object.Destructor = Dispose;
	string->Length = length;
	string->Chars = chars;
	string->Hash = hash;
	return (Obj*)string;
}

void StringImpl::Dispose(Obj* object) {
	ObjString* string = (ObjString*)object;

	Memory::Free(string->Chars);
}

bool StringImpl::VM_ElementGet(Obj* object, VMValue at, VMValue* result, Uint32 threadID) {
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
		*result = OBJECT_VAL(CopyString(&string->Chars[index], 1));
	}
	return true;
}
