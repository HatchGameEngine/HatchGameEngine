#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>

hsl_Object* hsl_array_new(hsl_Context* context) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return nullptr;
	}

	ObjArray* array = manager->NewArray();
	return (hsl_Object*)array;
}

hsl_Object* hsl_array_new_from_stack(hsl_Thread* thread, size_t count) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	if (vmThread->StackTop - count - 1 < vmThread->Stack) {
		return nullptr;
	}

	ObjArray* array = vmThread->Manager->NewArray();
	array->Values->reserve(count);
	for (int i = (int)count - 1; i >= 0; i--) {
		array->Values->push_back(vmThread->Peek(i));
	}
	for (int i = (int)count - 1; i >= 0; i--) {
		vmThread->Pop();
	}
	return (hsl_Object*)array;
}


hsl_Result hsl_array_push(hsl_Object* object, hsl_Value* value) {
	if (!object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)object)->Type != OBJ_ARRAY) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue vmValue = *(VMValue*)value;

	if (IS_LINKED_INTEGER(vmValue) || IS_LINKED_DECIMAL(vmValue) || IS_LOCATION(vmValue)) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjArray* array = (ObjArray*)object;

	array->Values->push_back(vmValue);

	return HSL_OK;
}

hsl_Result hsl_array_pop(hsl_Object* object) {
	if (!object) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)object)->Type != OBJ_ARRAY) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjArray* array = (ObjArray*)object;

	if (array->Values->size() == 0) {
		return HSL_INVALID_ARGUMENT;
	}

	array->Values->pop_back();

	return HSL_OK;
}

hsl_Value* hsl_array_get(hsl_Object* object, int index) {
	if (!object) {
		return nullptr;
	}

	if (((Obj*)object)->Type != OBJ_ARRAY) {
		return nullptr;
	}

	ObjArray* array = (ObjArray*)object;

	if (index < 0 || (Uint32)index >= array->Values->size()) {
		return nullptr;
	}

	return (hsl_Value*)(&(*array->Values)[index]);
}

hsl_Result hsl_array_set(hsl_Object* object, int index, hsl_Value* value) {
	if (!object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)object)->Type != OBJ_ARRAY) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjArray* array = (ObjArray*)object;

	if (index < 0 || (Uint32)index >= array->Values->size()) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue vmValue = *(VMValue*)value;

	if (IS_LINKED_INTEGER(vmValue) || IS_LINKED_DECIMAL(vmValue) || IS_LOCATION(vmValue)) {
		return HSL_INVALID_ARGUMENT;
	}

	(*array->Values)[index] = vmValue;

	return HSL_OK;
}

size_t hsl_array_size(hsl_Object* object) {
	if (!object) {
		return 0;
	}

	if (((Obj*)object)->Type != OBJ_ARRAY) {
		return 0;
	}

	ObjArray* array = (ObjArray*)object;

	return array->Values->size();
}
#endif
