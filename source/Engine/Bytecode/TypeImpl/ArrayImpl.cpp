#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/Types.h>

class ArrayImpl {
public:
    static ObjClass *Class;
};
#endif

#include <Engine/Bytecode/TypeImpl/ArrayImpl.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>

ObjClass* ArrayImpl::Class = nullptr;

PUBLIC STATIC void ArrayImpl::Init() {
    const char *name = "$$ArrayImpl";

    Class = NewClass(Murmur::EncryptString(name));
    Class->Name = CopyString(name);

    ScriptManager::DefineNative(Class, "iterate", ArrayImpl::VM_Iterate);
    ScriptManager::DefineNative(Class, "iteratorValue", ArrayImpl::VM_IteratorValue);

    ScriptManager::ClassImplList.push_back(Class);
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

PUBLIC STATIC VMValue ArrayImpl::VM_Iterate(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);

    ObjArray* array = GET_ARG(0, GetArray);

    if (array->Values->size() && IS_NULL(args[1]))
        return INTEGER_VAL(0);
    else {
        int iteration = GET_ARG(1, GetInteger) + 1;
        if (iteration >= 0 && iteration < array->Values->size())
            return INTEGER_VAL(iteration);
    }

    return NULL_VAL;
}

PUBLIC STATIC VMValue ArrayImpl::VM_IteratorValue(int argCount, VMValue* args, Uint32 threadID) {
    StandardLibrary::CheckArgCount(argCount, 2);

    ObjArray* array = GET_ARG(0, GetArray);
    int index = GET_ARG(1, GetInteger);
    if (index < 0 || (Uint32)index >= array->Values->size()) {
        ScriptManager::Threads[threadID].ThrowRuntimeError(false,
            "Index %d is out of bounds of array of size %d.", index, (int)array->Values->size());
        return NULL_VAL;
    }

    return (*array->Values)[index];
}
