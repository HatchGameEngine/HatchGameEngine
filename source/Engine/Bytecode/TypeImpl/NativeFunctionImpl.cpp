#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/NativeFunctionImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

/***
* \class NativeFunction
* \desc A function implemented natively, rather than in HSL.
*/

ObjClass* NativeFunctionImpl::Class = nullptr;

void NativeFunctionImpl::Init() {
	Class = NewClass("NativeFunction");
	Class->NewFn = Constructor;

	ScriptManager::DefineNative(Class, "Bind", VM_Bind);
	ScriptManager::DefineNative(Class, "BindArguments", VM_BindArguments);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(Class);
}

Obj* NativeFunctionImpl::Constructor() {
	throw ScriptException("Cannot directly construct NativeFunction!");
	return nullptr;
}

Obj* NativeFunctionImpl::New(NativeFn function) {
	ObjNativeFunction* native = (ObjNativeFunction*)AllocateObject(sizeof(ObjNativeFunction), OBJ_NATIVE_FUNCTION);
	Memory::Track(native, "NewNative");
	native->Object.Class = Class;
	native->Function = function;
	return (Obj*)native;
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

#define THROW_ERROR(...) ScriptManager::Threads[threadID].ThrowRuntimeError(false, __VA_ARGS__)

/***
 * \method Bind
 * \desc Binds a receiver, and optionally arguments, to the native function.
 * \param receiver (value): The receiver to bind.
 * \paramOpt ... (varargs): The arguments to bind.
 * \return <ref BoundMethod> Returns a bound method.
 * \ns NativeFunction
 */
VMValue NativeFunctionImpl::VM_Bind(int argCount, VMValue* args, Uint32 threadID) {
	if (argCount < 1) {
		StandardLibrary::CheckAtLeastArgCount(argCount, 1);
		return NULL_VAL;
	}
	else if (argCount > 255) {
		THROW_ERROR("Cannot bind more than 255 arguments.");
		return NULL_VAL;
	}

	ObjNativeFunction* native = GET_ARG(0, GetNativeFunction);
	ObjBoundMethod* bound = NewBoundMethod((Obj*)native, args + 1, argCount - 1);
	bound->HasReceiver = true;
	return OBJECT_VAL(bound);
}

/***
 * \method BindArguments
 * \desc Binds arguments to the native function.
 * \param ... (varargs): The arguments to bind.
 * \return <ref BoundMethod> Returns a bound method.
 * \ns Function
 */
VMValue NativeFunctionImpl::VM_BindArguments(int argCount, VMValue* args, Uint32 threadID) {
	if (argCount < 1) {
		StandardLibrary::CheckAtLeastArgCount(argCount, 1);
		return NULL_VAL;
	}
	else if (argCount > 255) {
		THROW_ERROR("Cannot bind more than 255 arguments.");
		return NULL_VAL;
	}

	ObjNativeFunction* native = GET_ARG(0, GetNativeFunction);
	return OBJECT_VAL(NewBoundMethod((Obj*)native, args + 1, argCount - 1));
}
