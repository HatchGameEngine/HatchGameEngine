#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Exceptions/ScriptException.h>

Obj* hsl_instance_new_internal(VMThread* vmThread, ObjClass* classObj) {
	if (classObj->NewFn) {
		try {
			return classObj->NewFn(vmThread);
		} catch (const ScriptException& error) {
			vmThread->ThrowRuntimeError(false, "%s", error.what());
			return nullptr;
		}
	}
	else {
		return (Obj*)vmThread->Manager->NewInstance(classObj);
	}
}

hsl_Object* hsl_instance_new(hsl_Thread* thread, hsl_Object* klass) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !klass) {
		return nullptr;
	}

	if (((Obj*)klass)->Type != OBJ_CLASS) {
		return nullptr;
	}

	Obj* instance = hsl_instance_new_internal(vmThread, (ObjClass*)klass);
	return (hsl_Object*)instance;
}

hsl_Result hsl_instance_new_from_stack(hsl_Thread* thread, size_t num_args) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackTop - num_args - 1 < vmThread->Stack) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue callee = vmThread->Peek(num_args);
	if (!IS_OBJECT(callee) || OBJECT_TYPE(callee) != OBJ_CLASS) {
		return HSL_COULD_NOT_CALL;
	}

	ObjClass* klass = AS_CLASS(callee);
	Obj* instance = hsl_instance_new_internal(vmThread, (ObjClass*)klass);
	if (instance == nullptr) {
		vmThread->StackTop[-num_args - 1] = NULL_VAL;
		return HSL_COULD_NOT_INSTANTIATE;
	}

	vmThread->StackTop[-num_args - 1] = OBJECT_VAL(instance);

	if (HasInitializer(klass)) {
		Obj* callable = AS_OBJECT(klass->Initializer);
		return hsl_invoke_callable(thread, (hsl_Object*)callable, num_args);
	}
	else if (num_args != 0) {
		return HSL_ARG_COUNT_MISMATCH;
	}

	return HSL_OK;
}
#endif
