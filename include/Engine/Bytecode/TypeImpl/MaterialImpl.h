#ifndef ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class MaterialImpl {
public:
	static ObjClass* Class;

	static void Init();
	static Obj* VM_New(void);
	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	static bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);
	static bool VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID);

	static Uint32 Hash_Name;

	static Uint32 Hash_DiffuseRed;
	static Uint32 Hash_DiffuseGreen;
	static Uint32 Hash_DiffuseBlue;
	static Uint32 Hash_DiffuseAlpha;
	static Uint32 Hash_DiffuseTexture;

	static Uint32 Hash_SpecularRed;
	static Uint32 Hash_SpecularGreen;
	static Uint32 Hash_SpecularBlue;
	static Uint32 Hash_SpecularAlpha;
	static Uint32 Hash_SpecularTexture;

	static Uint32 Hash_AmbientRed;
	static Uint32 Hash_AmbientGreen;
	static Uint32 Hash_AmbientBlue;
	static Uint32 Hash_AmbientAlpha;
	static Uint32 Hash_AmbientTexture;

#ifdef MATERIAL_EXPOSE_EMISSIVE
	static Uint32 Hash_EmissiveRed;
	static Uint32 Hash_EmissiveGreen;
	static Uint32 Hash_EmissiveBlue;
	static Uint32 Hash_EmissiveAlpha;
	static Uint32 Hash_EmissiveTexture;
#endif
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H */
