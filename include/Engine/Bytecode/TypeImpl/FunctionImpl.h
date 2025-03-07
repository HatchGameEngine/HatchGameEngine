#ifndef ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class FunctionImpl {
public:
	static ObjClass* Class;

	static void Init();
	static VMValue VM_Bind(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H */
