#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* FunctionImpl::Class = nullptr;

void FunctionImpl::Init() {
	Class = NewClass(CLASS_FUNCTION);

#ifdef HSL_VM
	ScriptManager::DefineNative(Class, "bind", FunctionImpl::VM_Bind);
#endif

	TypeImpl::RegisterClass(Class);
}

Obj* FunctionImpl::New() {
	ObjFunction* function = (ObjFunction*)AllocateObject(sizeof(ObjFunction), OBJ_FUNCTION);
	Memory::Track(function, "NewFunction");
	function->Object.Class = Class;
	function->Chunk.Init();
	return (Obj*)function;
}

#ifdef HSL_VM
#define GET_ARG(argIndex, argFunction) (ScriptManager::argFunction(args, argIndex, threadID))

VMValue FunctionImpl::VM_Bind(int argCount, VMValue* args, Uint32 threadID) {
	ScriptManager::CheckArgCount(argCount, 2);

	ObjFunction* function = GET_ARG(0, GetFunction);

	return OBJECT_VAL(NewBoundMethod(args[1], function));
}
#endif
