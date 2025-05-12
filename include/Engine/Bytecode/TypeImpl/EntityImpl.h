#ifndef ENGINE_BYTECODE_TYPEIMPL_ENTITYIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_ENTITYIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class EntityImpl {
public:
	static ObjClass* Class;

	static void Init();

	static void Dispose(Obj* object);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_ENTITYIMPL_H */
