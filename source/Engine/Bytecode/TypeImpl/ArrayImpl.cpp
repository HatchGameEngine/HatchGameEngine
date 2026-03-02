#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ArrayImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ArrayImpl::ArrayImpl(ScriptManager* manager) {
	Manager = manager;
	Class = Manager->NewClass(CLASS_ARRAY);

#ifdef HSL_VM
	Manager->DefineNative(Class, "iterate", VM_Iterate);
	Manager->DefineNative(Class, "iteratorValue", VM_IteratorValue);
#endif

	TypeImpl::RegisterClass(manager, Class);
}

Obj* ArrayImpl::New() {
	ObjArray* array = (ObjArray*)Manager->AllocateObject(sizeof(ObjArray), OBJ_ARRAY);
	Memory::Track(array, "NewArray");
	array->Object.Class = Class;
	array->Values = new vector<VMValue>();
	return (Obj*)array;
}

void ArrayImpl::Dispose(Obj* object) {
	ObjArray* array = (ObjArray*)object;

	// An array does not own its values, so it's not allowed to free them.
	array->Values->clear();

	delete array->Values;
}

#ifdef HSL_VM
#define GET_ARG(argIndex, argFunction) (thread->Manager->argFunction(args, argIndex, thread))

VMValue ArrayImpl::VM_Iterate(int argCount, VMValue* args, VMThread* thread) {
	ScriptManager::CheckArgCount(argCount, 2, thread);

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

VMValue ArrayImpl::VM_IteratorValue(int argCount, VMValue* args, VMThread* thread) {
	ScriptManager::CheckArgCount(argCount, 2, thread);

	ObjArray* array = GET_ARG(0, GetArray);
	int index = GET_ARG(1, GetInteger);
	if (index < 0 || (Uint32)index >= array->Values->size()) {
		thread->ThrowRuntimeError(false,
			"Index %d is out of bounds of array of size %d.",
			index,
			(int)array->Values->size());
		return NULL_VAL;
	}

	return (*array->Values)[index];
}
#endif
