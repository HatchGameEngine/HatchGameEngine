#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>

ObjClass* FunctionImpl::Class = nullptr;

void FunctionImpl::Init() {
	const char* name = "$$FunctionImpl";

	Class = NewClass(Murmur::EncryptString(name));
	Class->Name = CopyString(name);

	ScriptManager::DefineNative(Class, "bind", FunctionImpl::VM_Bind);

	ScriptManager::ClassImplList.push_back(Class);
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

VMValue FunctionImpl::VM_Bind(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjFunction* function = GET_ARG(0, GetFunction);

	return OBJECT_VAL(NewBoundMethod(args[1], function));
}
