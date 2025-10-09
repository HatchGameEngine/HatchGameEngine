#ifndef ENGINE_BYTECODE_TYPEIMPL_STREAMIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_STREAMIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define CLASS_STREAM "$$StreamImpl"

#define IS_STREAM(value) IsNativeInstance(value, CLASS_STREAM)
#define AS_STREAM(value) ((ObjStream*)AS_OBJECT(value))

class StreamImpl {
public:
	static ObjClass* Class;

	static void Init();

	static ObjStream* New(void* streamPtr, bool writable);
	static void Dispose(Obj* object);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_STREAMIMPL_H */
