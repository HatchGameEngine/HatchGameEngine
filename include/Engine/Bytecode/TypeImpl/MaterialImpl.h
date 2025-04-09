#ifndef ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

namespace MaterialImpl {
//public:
	extern ObjClass* Class;

	extern Uint32 Hash_Name;

	extern Uint32 Hash_DiffuseRed;
	extern Uint32 Hash_DiffuseGreen;
	extern Uint32 Hash_DiffuseBlue;
	extern Uint32 Hash_DiffuseAlpha;
	extern Uint32 Hash_DiffuseTexture;

	extern Uint32 Hash_SpecularRed;
	extern Uint32 Hash_SpecularGreen;
	extern Uint32 Hash_SpecularBlue;
	extern Uint32 Hash_SpecularAlpha;
	extern Uint32 Hash_SpecularTexture;

	extern Uint32 Hash_AmbientRed;
	extern Uint32 Hash_AmbientGreen;
	extern Uint32 Hash_AmbientBlue;
	extern Uint32 Hash_AmbientAlpha;
	extern Uint32 Hash_AmbientTexture;

#ifdef MATERIAL_EXPOSE_EMISSIVE
	extern Uint32 Hash_EmissiveRed;
	extern Uint32 Hash_EmissiveGreen;
	extern Uint32 Hash_EmissiveBlue;
	extern Uint32 Hash_EmissiveAlpha;
	extern Uint32 Hash_EmissiveTexture;
#endif

	void Init();
	Obj* VM_New(void);
	VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);
	bool VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_MATERIALIMPL_H */
