#ifndef ENGINE_BYTECODE_TYPEIMPL_TEXTUREIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_TEXTUREIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define CLASS_TEXTURE "Texture"

#define IS_TEXTURE(value) IsNativeInstance(value, CLASS_TEXTURE)
#define AS_TEXTURE(value) ((ObjTexture*)AS_OBJECT(value))

class TextureImpl {
public:
	static ObjClass* Class;

	static void Init();

	static Obj* New(void);
	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	static void Dispose(Obj* object);

	static bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);
	static bool VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID);

	static void* GetTexture(ObjTexture* object);
	static ObjTexture* GetTextureObject(void* texture, bool isViewTexture);

	static VMValue VM_SetSize(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Scale(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetPixel(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetPixelData(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetPixel(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_CopyPixels(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Convert(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Apply(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Delete(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_CanConvertBetweenFormats(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_FormatHasAlphaChannel(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_TEXTUREIMPL_H */
