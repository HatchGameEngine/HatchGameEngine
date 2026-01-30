#ifndef ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_SPRITEIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_SPRITEIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

VMValue SpriteImpl_GetAnimationLoopFrame(int argCount, VMValue* args, Uint32 threadID);
VMValue SpriteImpl_GetAnimationFrameCount(int argCount, VMValue* args, Uint32 threadID);
VMValue SpriteImpl_GetAnimationSpeed(int argCount, VMValue* args, Uint32 threadID);

class SpriteImpl {
private:
	static void AddNatives();
	static bool IsValidField(Uint32 hash);

public:
	static ObjClass* Class;

	static void Init();
	static bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);
	static bool VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_RESOURCEIMPL_SPRITEIMPL_H */
