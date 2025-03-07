#ifndef ENGINE_BYTECODE_TYPEIMPL_STRINGIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_STRINGIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class StringImpl {
public:
	static ObjClass* Class;

	static void Init();
	static bool VM_ElementGet(Obj* object, VMValue at, VMValue* result, Uint32 threadID);
	static bool VM_ElementSet(Obj* object, VMValue at, VMValue value, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_STRINGIMPL_H */
