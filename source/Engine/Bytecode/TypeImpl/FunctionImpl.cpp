#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* FunctionImpl::Class = nullptr;

void FunctionImpl::Init() {
	Class = NewClass(CLASS_FUNCTION);

	ScriptManager::DefineNative(Class, "bind", FunctionImpl::VM_Bind);

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

VMValue FunctionImpl::VM_Bind(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjFunction* function = GET_ARG(0, GetFunction);

	return OBJECT_VAL(NewBoundMethod(args[1], function));
}
