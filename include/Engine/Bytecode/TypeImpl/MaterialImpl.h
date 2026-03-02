#ifndef ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

class ScriptManager;

#define IS_MATERIAL(value) IsNativeInstance(value, CLASS_MATERIAL)
#define AS_MATERIAL(value) ((ObjMaterial*)AS_OBJECT(value))

class MaterialImpl : public TypeImpl {
private:
	Uint32 Hash_Name = 0;

	Uint32 Hash_DiffuseRed = 0;
	Uint32 Hash_DiffuseGreen = 0;
	Uint32 Hash_DiffuseBlue = 0;
	Uint32 Hash_DiffuseAlpha = 0;
	Uint32 Hash_DiffuseTexture = 0;

	Uint32 Hash_SpecularRed = 0;
	Uint32 Hash_SpecularGreen = 0;
	Uint32 Hash_SpecularBlue = 0;
	Uint32 Hash_SpecularAlpha = 0;
	Uint32 Hash_SpecularTexture = 0;

	Uint32 Hash_AmbientRed = 0;
	Uint32 Hash_AmbientGreen = 0;
	Uint32 Hash_AmbientBlue = 0;
	Uint32 Hash_AmbientAlpha = 0;
	Uint32 Hash_AmbientTexture = 0;

#ifdef MATERIAL_EXPOSE_EMISSIVE
	Uint32 Hash_EmissiveRed = 0;
	Uint32 Hash_EmissiveGreen = 0;
	Uint32 Hash_EmissiveBlue = 0;
	Uint32 Hash_EmissiveAlpha = 0;
	Uint32 Hash_EmissiveTexture = 0;
#endif

public:
	MaterialImpl(ScriptManager* manager);

	static Obj* New(VMThread* thread);
	ObjMaterial* New(void* materialPtr);
	static void Dispose(Obj* object);

	static VMValue VM_Initializer(int argCount, VMValue* args, VMThread* thread);
	static bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, VMThread* thread);
	static bool VM_PropertySet(Obj* object, Uint32 hash, VMValue value, VMThread* thread);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H */
