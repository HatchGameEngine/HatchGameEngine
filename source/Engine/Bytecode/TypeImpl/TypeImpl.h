#ifndef ENGINE_BYTECODE_TYPEIMPL_TYPEIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_TYPEIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class TypeImpl {
public:
	static void Init();

	static void RegisterClass(ObjClass* klass);
	static void ExposeClass(ObjClass* klass);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_TYPEIMPL_H */
