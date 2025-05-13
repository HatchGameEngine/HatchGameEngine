#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ArrayImpl.h>

ObjClass* ArrayImpl::Class = nullptr;

void ArrayImpl::Init() {
	Class = NewClass(CLASS_ARRAY);

	ScriptManager::DefineNative(Class, "iterate", ArrayImpl::VM_Iterate);
	ScriptManager::DefineNative(Class, "iteratorValue", ArrayImpl::VM_IteratorValue);

	ScriptManager::ClassImplList.push_back(Class);
}

Obj* ArrayImpl::New() {
	ObjArray* array = (ObjArray*)AllocateObject(sizeof(ObjArray), OBJ_ARRAY);
	Memory::Track(array, "NewArray");
	array->Object.Class = Class;
	array->Object.Destructor = Dispose;
	array->Values = new vector<VMValue>();
	return (Obj*)array;
}

void ArrayImpl::Dispose(Obj* object) {
	ObjArray* array = (ObjArray*)object;

	// An array does not own its values, so it's not allowed to free them.
	array->Values->clear();

	delete array->Values;
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

VMValue ArrayImpl::VM_Iterate(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjArray* array = GET_ARG(0, GetArray);

	if (array->Values->size() && IS_NULL(args[1])) {
		return INTEGER_VAL(0);
	}
	else if (!IS_NULL(args[1])) {
		int iteration = GET_ARG(1, GetInteger) + 1;
		if (iteration >= 0 && iteration < array->Values->size()) {
			return INTEGER_VAL(iteration);
		}
	}

	return NULL_VAL;
}

VMValue ArrayImpl::VM_IteratorValue(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjArray* array = GET_ARG(0, GetArray);
	int index = GET_ARG(1, GetInteger);
	if (index < 0 || (Uint32)index >= array->Values->size()) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false,
			"Index %d is out of bounds of array of size %d.",
			index,
			(int)array->Values->size());
		return NULL_VAL;
	}

	return (*array->Values)[index];
}
