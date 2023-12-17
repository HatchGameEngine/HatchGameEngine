#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/Types.h>

class StringImpl {
public:
    static ObjClass *Class;
};
#endif

#include <Engine/Bytecode/TypeImpl/StringImpl.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>

ObjClass* StringImpl::Class = nullptr;

PUBLIC STATIC void StringImpl::Init() {
    const char *name = "$$StringImpl";

    Class = NewClass(Murmur::EncryptString(name));
    Class->Name = CopyString(name);
    Class->ElementGet = StringImpl::VM_ElementGet;
    Class->ElementSet = StringImpl::VM_ElementSet;

    ScriptManager::ClassImplList.push_back(Class);
}

#define THROW_ERROR(...) ScriptManager::Threads[threadID].ThrowRuntimeError(false, __VA_ARGS__)

PUBLIC STATIC bool StringImpl::VM_ElementGet(Obj* object, VMValue at, VMValue* result, Uint32 threadID) {
    ObjString* string = (ObjString*)object;

    if (!IS_INTEGER(at)) {
        THROW_ERROR("Cannot get value from array using non-Integer value as an index.");
        if (result)
            *result = NULL_VAL;
        return true;
    }

    int index = AS_INTEGER(at);
    if (index < 0)
        index = string->Length + index;

    if (index >= string->Length) {
        THROW_ERROR("Index %d is out of bounds of string of length %d.", index, (int)string->Length);
        if (result)
            *result = NULL_VAL;
        return true;
    }

    if (result)
        *result = OBJECT_VAL(CopyString(&string->Chars[index], 1));
    return true;
}
PUBLIC STATIC bool StringImpl::VM_ElementSet(Obj* object, VMValue at, VMValue value, Uint32 threadID) {
    ObjString* string = (ObjString*)object;

    if (!IS_INTEGER(at)) {
        THROW_ERROR("Cannot set value from array using non-Integer value as an index.");
        return true;
    }

    int index = AS_INTEGER(at);
    if (index < 0)
        index = string->Length + index;

    if (index >= string->Length) {
        THROW_ERROR("Index %d is out of bounds of string of length %d.", index, (int)string->Length);
        return true;
    }

    if (IS_INTEGER(value)) {
        int chr = AS_INTEGER(value);
        string->Chars[index] = (Uint8)chr;
    }
    else if (IS_STRING(value)) {
        ObjString* str = AS_STRING(value);
        if (str->Length > 1)
            THROW_ERROR("Cannot modify string character a String value longer than a single character.");
        if (str->Length > 0)
            string->Chars[index] = str->Chars[0];
    }
    else {
        THROW_ERROR("Cannot modify string character using non-Integer or non-String value.");
    }

    return true;
}
