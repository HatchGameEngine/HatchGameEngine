#ifndef ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

namespace FunctionImpl {
//public:
	extern ObjClass* Class;

	void Init();
	VMValue VM_Bind(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H */
