#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Utilities/StringUtils.h>

hsl_Result hsl_push_integer(hsl_Thread* thread, int value) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	vmThread->Push(INTEGER_VAL(value));

	return HSL_OK;
}

hsl_Result hsl_push_decimal(hsl_Thread* thread, float value) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	vmThread->Push(DECIMAL_VAL(value));

	return HSL_OK;
}

hsl_Result hsl_push_string(hsl_Thread* thread, const char* value) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	ObjString* string = vmThread->Manager->CopyString(value);

	vmThread->Push(OBJECT_VAL(string));

	return HSL_OK;
}

hsl_Result hsl_push_string_sized(hsl_Thread* thread, const char* value, size_t sz) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || (!value && sz > 0)) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	ObjString* string = vmThread->Manager->CopyString(value, sz);
	if (!string) {
		return HSL_OUT_OF_MEMORY;
	}

	vmThread->Push(OBJECT_VAL(string));

	return HSL_OK;
}

hsl_Result hsl_push_object(hsl_Thread* thread, hsl_Object* object) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	vmThread->Push(OBJECT_VAL((Obj*)object));

	return HSL_OK;
}

hsl_Result hsl_push_null(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	vmThread->Push(NULL_VAL);

	return HSL_OK;
}

int hsl_pop_integer(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return 0;
	}

	if (vmThread->StackTop == vmThread->Stack) {
		return 0;
	}

	VMValue value = vmThread->Pop();
	if (IS_INTEGER(value)) {
		return AS_INTEGER(value);
	}

	return 0;
}

float hsl_pop_decimal(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return 0.0f;
	}

	if (vmThread->StackTop == vmThread->Stack) {
		return 0.0f;
	}

	VMValue value = vmThread->Pop();
	if (IS_DECIMAL(value)) {
		return AS_DECIMAL(value);
	}

	return 0.0f;
}

char* hsl_pop_string(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	if (vmThread->StackTop == vmThread->Stack) {
		return nullptr;
	}

	VMValue value = vmThread->Pop();
	if (IS_STRING(value)) {
		return StringUtils::Duplicate(AS_CSTRING(value));
	}

	return nullptr;
}

hsl_Object* hsl_pop_object(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	if (vmThread->StackTop == vmThread->Stack) {
		return nullptr;
	}

	VMValue value = vmThread->Pop();
	if (IS_OBJECT(value)) {
		return (hsl_Object*)AS_OBJECT(value);
	}

	return nullptr;
}

hsl_Result hsl_pop(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackTop == vmThread->Stack) {
		return HSL_STACK_UNDERFLOW;
	}

	vmThread->Pop();

	return HSL_OK;
}

hsl_Value* hsl_stack_get(hsl_Thread* thread, size_t index) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	VMValue* stackPointer = vmThread->StackTop - index - 1;
	if (stackPointer < vmThread->Stack) {
		return nullptr;
	}

	return (hsl_Value*)stackPointer;
}

hsl_Result hsl_stack_set(hsl_Thread* thread, hsl_Value* value, size_t index) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue* stackPointer = vmThread->StackTop - index - 1;
	if (stackPointer < vmThread->Stack) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue vmValue = *(VMValue*)value;

	if (IS_LINKED_INTEGER(vmValue) || IS_LINKED_DECIMAL(vmValue)) {
		return HSL_INVALID_ARGUMENT;
	}

	*stackPointer = vmValue;

	return HSL_OK;
}

hsl_Result hsl_stack_copy(hsl_Thread* thread, size_t index, hsl_Value* dest) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue* stackPointer = vmThread->StackTop - index - 1;
	if (stackPointer < vmThread->Stack) {
		return HSL_INVALID_ARGUMENT;
	}

	memcpy((VMValue*)dest, stackPointer, sizeof(VMValue));

	return HSL_OK;
}

size_t hsl_stack_size(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return 0;
	}

	return vmThread->StackSize();
}
#endif
