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

	static void* GetTexture(ObjTexture* object);
	static ObjTexture* GetTextureObject(void* texture);

	static VMValue VM_CopyPixels(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Apply(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Delete(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_TEXTUREIMPL_H */
