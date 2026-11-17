#ifndef ENGINE_BYTECODE_TYPEIMPL_NATIVEFUNCTIONIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_NATIVEFUNCTIONIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class NativeFunctionImpl {
public:
	static ObjClass* Class;

	static void Init();

	static Obj* Constructor();
	static Obj* New(NativeFn function);

	static VMValue VM_Bind(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_BindArguments(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_NATIVEFUNCTIONIMPL_H */
