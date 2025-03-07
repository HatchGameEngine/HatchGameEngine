#ifndef ENGINE_BYTECODE_TYPEIMPL_MAPIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_MAPIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class MapImpl {
public:
	static ObjClass* Class;

	static void Init();
	static VMValue VM_GetKeys(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Iterate(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_IteratorValue(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_MAPIMPL_H */
