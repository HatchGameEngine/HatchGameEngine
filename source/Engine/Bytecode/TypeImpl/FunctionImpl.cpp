#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

/***
* \class Function
* \desc A function, defined in a HSL script.
*/

ObjClass* FunctionImpl::Class = nullptr;

void FunctionImpl::Init() {
	Class = NewClass("Function");
	Class->NewFn = Constructor;

	ScriptManager::DefineNative(Class, "Bind", FunctionImpl::VM_Bind);
	ScriptManager::DefineNative(Class, "bind", FunctionImpl::VM_Bind);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(Class);
}

Obj* FunctionImpl::Constructor() {
	throw ScriptException("Cannot directly construct Function!");
	return nullptr;
}

Obj* FunctionImpl::New() {
	ObjFunction* function = (ObjFunction*)AllocateObject(sizeof(ObjFunction), OBJ_FUNCTION);
	Memory::Track(function, "NewFunction");
	function->Object.Class = Class;
	function->Chunk.Init();
	return (Obj*)function;
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

/***
 * \method Bind
 * \desc Binds a receiver to a method.
 * \param receiver (value): The receiver to bind.
 * \return <ref BoundMethod> Returns a bound method.
 * \ns Function
 */
VMValue FunctionImpl::VM_Bind(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjFunction* function = GET_ARG(0, GetFunction);

	return OBJECT_VAL(NewBoundMethod(args[1], function));
}
