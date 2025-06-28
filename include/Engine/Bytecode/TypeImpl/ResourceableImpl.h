#ifndef ENGINE_BYTECODE_TYPEIMPL_RESOURCEABLEIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_RESOURCEABLEIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class ResourceableImpl {
public:
	static ObjClass* Class;

	static void Init();
	static void* New(void* ptr);
	static void Dispose(Obj* object);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_RESOURCEABLEIMPL_H */
