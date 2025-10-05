#ifndef ENGINE_BYTECODE_TYPEIMPL_RESOURCEABLEIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_RESOURCEABLEIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define GET_RESOURCEABLE(object) (Resourceable*)(((ObjResourceable*)object)->ResourceablePtr)

class ResourceableImpl {
public:
	static ObjClass* Class;

	static void Init();
	static void* New(void* ptr);
	static void Dispose(Obj* object);

	static ValueGetFn GetGetter(Uint8 type);
	static ValueSetFn GetSetter(Uint8 type);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_RESOURCEABLEIMPL_H */
