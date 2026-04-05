#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* FunctionImpl::Class = nullptr;

void FunctionImpl::Init() {
	Class = NewClass(CLASS_FUNCTION);

	ScriptManager::DefineNative(Class, "bind", FunctionImpl::VM_Bind);
	ScriptManager::DefineNative(Class, "bindArguments", FunctionImpl::VM_BindArguments);

	TypeImpl::RegisterClass(Class);
}

Obj* FunctionImpl::New() {
	ObjFunction* function = (ObjFunction*)AllocateObject(sizeof(ObjFunction), OBJ_FUNCTION);
	Memory::Track(function, "NewFunction");
	function->Object.Class = Class;
	function->Chunk.Init();
	return (Obj*)function;
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

#define THROW_ERROR(...) ScriptManager::Threads[threadID].ThrowRuntimeError(false, __VA_ARGS__)

VMValue FunctionImpl::VM_Bind(int argCount, VMValue* args, Uint32 threadID) {
	if (argCount < 1) {
		StandardLibrary::CheckAtLeastArgCount(argCount, 1);
		return NULL_VAL;
	}

	ObjFunction* function = GET_ARG(0, GetFunction);

	if (argCount > 2) {
		if (argCount - 2 > function->Arity) {
			THROW_ERROR("Expected at most %d arguments, but received %d.",
				function->Arity + 1,
				argCount - 1);
			return NULL_VAL;
		}
		else if (argCount - 2 < function->MinArity) {
			THROW_ERROR("Expected at least %d arguments, but received only %d.",
				function->MinArity + 1,
				argCount - 1);
			return NULL_VAL;
		}
	}

	ObjBoundMethod* bound = NewBoundMethod(function, args + 1, argCount - 1);
	bound->HasReceiver = true;
	return OBJECT_VAL(bound);
}

VMValue FunctionImpl::VM_BindArguments(int argCount, VMValue* args, Uint32 threadID) {
	if (argCount < 1) {
		StandardLibrary::CheckAtLeastArgCount(argCount, 1);
		return NULL_VAL;
	}

	ObjFunction* function = GET_ARG(0, GetFunction);

	if (argCount - 1 > function->Arity) {
		THROW_ERROR("Expected at most %d arguments, but received %d.",
			function->Arity,
			argCount - 1);
		return NULL_VAL;
	}
	else if (argCount - 1 < function->MinArity) {
		THROW_ERROR("Expected at least %d arguments, but received only %d.",
			function->MinArity,
			argCount - 1);
		return NULL_VAL;
	}

	return OBJECT_VAL(NewBoundMethod(function, args + 1, argCount - 1));
}
