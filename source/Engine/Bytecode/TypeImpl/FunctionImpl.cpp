#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

FunctionImpl::FunctionImpl(ScriptManager* manager) {
	Manager = manager;
	Class = Manager->NewClass(CLASS_FUNCTION);

#ifdef HSL_VM
	Manager->DefineNative(Class, "bind", VM_Bind);
#endif

	TypeImpl::RegisterClass(manager, Class);
}

Obj* FunctionImpl::New() {
	ObjFunction* function = (ObjFunction*)Manager->AllocateObject(sizeof(ObjFunction), OBJ_FUNCTION);
	Memory::Track(function, "NewFunction");
	function->Object.Class = Class;
	function->Chunk.Init();
	return (Obj*)function;
}

#ifdef HSL_VM
#define GET_ARG(argIndex, argFunction) (thread->Manager->argFunction(args, argIndex, thread))

VMValue FunctionImpl::VM_Bind(int argCount, VMValue* args, VMThread* thread) {
	ScriptManager::CheckArgCount(argCount, 2, thread);

	ObjFunction* function = GET_ARG(0, GetFunction);

	return OBJECT_VAL(thread->Manager->NewBoundMethod(args[1], function));
}
#endif
