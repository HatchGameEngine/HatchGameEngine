#ifndef ENGINE_BYTECODE_TYPEIMPL_INSTANCEMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_INSTANCEMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class InstanceImpl {
public:
	static Obj* New(size_t size, ObjType type);
	static void Dispose(Obj* object);
	static void CopyFields(ObjInstance* dest, ObjInstance* src);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_INSTANCEMPL_H */
