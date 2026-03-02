#ifndef ENGINE_BYTECODE_TYPEIMPL_STREAMIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_STREAMIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

class ScriptManager;

#define IS_STREAM(value) IsNativeInstance(value, CLASS_STREAM)
#define AS_STREAM(value) ((ObjStream*)AS_OBJECT(value))

class StreamImpl : public TypeImpl {
public:
	StreamImpl(ScriptManager* manager);

	ObjStream* New(void* streamPtr, bool writable);
	static void Dispose(Obj* object);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_STREAMIMPL_H */
