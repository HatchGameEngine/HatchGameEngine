#ifndef ENGINE_BYTECODE_TYPEIMPL_INSTANCEMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_INSTANCEMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class InstanceImpl {
public:
	static ObjClass* Class;

	static void Init();

	static void Dispose(Obj* object);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_INSTANCEMPL_H */
