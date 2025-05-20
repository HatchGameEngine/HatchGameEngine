#ifndef ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class ResourceImpl {
public:
	static ObjClass* Class;

	static void Init();
	static Obj* VM_New(void);
	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	static bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_H */
