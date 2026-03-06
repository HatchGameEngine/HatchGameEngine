#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Bytecode/VMThread.h>

hsl_Result hsl_setup_call_frame(hsl_Thread* thread, hsl_Function* function, size_t num_args) {
	VMThread* vmThread = (VMThread*)thread;
	ObjFunction* objFunction = (ObjFunction*)function;

	if (!vmThread || !objFunction) {
		return HSL_INVALID_ARGUMENT;
	}

	if (objFunction->MinArity < objFunction->Arity) {
		if (num_args < objFunction->MinArity) {
			return HSL_NOT_ENOUGH_ARGS;
		}
		else if (num_args > objFunction->Arity) {
			return HSL_ARG_COUNT_MISMATCH;
		}
	}
	else if (num_args != objFunction->Arity) {
		return HSL_ARG_COUNT_MISMATCH;
	}

	if (vmThread->FrameCount == FRAMES_MAX) {
		return HSL_CALL_FRAME_OVERFLOW;
	}

	if (!vmThread->Call(objFunction, num_args)) {
		return HSL_COULD_NOT_CALL;
	}

	vmThread->ReturnFrame = vmThread->FrameCount - 1;

	return HSL_OK;
}

hsl_Result hsl_run_call_frame(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->FrameCount == 0) {
		return HSL_NO_CALL_FRAMES;
	}

	hsl_Result result = HSL_OK;

	while (true) {
		int ret = vmThread->RunInstruction();
		if (ret == INTERPRET_FINISHED) {
			break;
		}
		else if (ret != INTERPRET_OK) {
			result = HSL_RUNTIME_ERROR;
			break;
		}
#ifdef VM_DEBUG
		else if (vmThread->FrameCount == 0) {
			break;
		}
#endif
	}

	return result;
}

hsl_Result hsl_run_call_frame_instruction(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->FrameCount == 0) {
		return HSL_NO_CALL_FRAMES;
	}

	int ret = vmThread->RunInstruction();
	if (ret != INTERPRET_OK && ret != INTERPRET_FINISHED) {
		return HSL_RUNTIME_ERROR;
	}

	return HSL_OK;
}

hsl_Result hsl_call(hsl_Thread* thread, size_t num_args) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackTop - num_args - 1 < vmThread->Stack) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue callable = vmThread->Peek(num_args);
	if (!IS_CALLABLE(callable)) {
		return HSL_COULD_NOT_CALL;
	}

	if (!vmThread->CallValue(callable, num_args)) {
		return HSL_COULD_NOT_CALL;
	}

	if (IS_NATIVE_FUNCTION(callable) || IS_API_NATIVE_FUNCTION(callable)) {
		vmThread->InterpretResult = vmThread->Pop();

		return HSL_OK;
	}

	return hsl_run_call_frame(thread);
}

hsl_Value* hsl_get_result(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	return (hsl_Value*)(&vmThread->InterpretResult);
}
#endif
