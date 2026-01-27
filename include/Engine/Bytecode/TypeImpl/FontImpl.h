#ifndef ENGINE_BYTECODE_TYPEIMPL_FONTIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_FONTIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define IS_FONT(value) IsNativeInstance(value, CLASS_FONT)
#define AS_FONT(value) ((ObjFont*)AS_OBJECT(value))

class FontImpl {
public:
	static ObjClass* Class;

	static void Init();

	static Obj* New(void);
	static ObjFont* New(void* fontPtr);
	static void Dispose(Obj* object);

	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetPixelsPerUnit(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetAscent(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetDescent(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLeading(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetSpaceWidth(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetOversampling(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetPixelCoverageThreshold(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetGlyphAdvance(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_IsAntialiasingEnabled(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_HasGlyph(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetPixelsPerUnit(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetAscent(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetDescent(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetLeading(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetSpaceWidth(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetOversampling(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetPixelCoverageThreshold(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetAntialiasing(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_FONTIMPL_H */
