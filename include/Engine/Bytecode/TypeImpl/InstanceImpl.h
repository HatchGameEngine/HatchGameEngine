#ifndef ENGINE_BYTECODE_TYPEIMPL_INSTANCEMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_INSTANCEMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

class ScriptManager;

class InstanceImpl : public TypeImpl {
public:
	InstanceImpl(ScriptManager* manager);

	Obj* New(size_t size, ObjType type);
	static void Dispose(Obj* object);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_INSTANCEMPL_H */
