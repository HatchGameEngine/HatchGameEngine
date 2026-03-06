#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/VMThread.h>

hsl_Result hsl_invoke(hsl_Thread* thread, const char* name, size_t num_args) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackTop - num_args - 1 < vmThread->Stack) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	int status = vmThread->Invoke(vmThread->Peek(num_args), num_args, hash);
	if (status != INVOKE_OK) {
		return HSL_COULD_NOT_CALL;
	}

	return HSL_OK;
}

hsl_Result hsl_invoke_callable(hsl_Thread* thread, hsl_Object* callable, size_t num_args) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !callable) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue callableValue = OBJECT_VAL(callable);
	if (!IS_CALLABLE(callableValue)) {
		return HSL_COULD_NOT_CALL;
	}

	if (!vmThread->CallForObject(callableValue, num_args)) {
		return HSL_COULD_NOT_CALL;
	}

	return HSL_OK;
}

int hsl_callable_get_arity(hsl_Object* callable) {
	if (!callable) {
		return 0;
	}

	int minArity, maxArity;
	if (ScriptManager::GetArity(OBJECT_VAL(callable), minArity, maxArity)) {
		return maxArity;
	}
	return 0;
}

int hsl_callable_get_min_arity(hsl_Object* callable) {
	if (!callable) {
		return 0;
	}

	int minArity, maxArity;
	if (ScriptManager::GetArity(OBJECT_VAL(callable), minArity, maxArity)) {
		return minArity;
	}
	return 0;
}
#endif
