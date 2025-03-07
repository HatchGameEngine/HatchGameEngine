#ifndef ENGINE_BYTECODE_TYPEIMPL_ARRAYIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_ARRAYIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class ArrayImpl {
public:
	static ObjClass* Class;

	static void Init();
	static VMValue VM_Iterate(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_IteratorValue(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_ARRAYIMPL_H */
