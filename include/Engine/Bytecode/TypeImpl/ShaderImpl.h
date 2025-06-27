#ifndef ENGINE_BYTECODE_TYPEIMPL_SHADERLIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_SHADERLIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class ShaderImpl {
public:
	static ObjClass* Class;

	static void Init();
	static Obj* VM_New(void);
	static bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);

	static VMValue VM_HasStage(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_CanCompile(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_IsValid(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_AddStage(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_AddStageFromString(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_AssignTextureUnit(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetTextureUnit(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_HasUniform(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetUniformType(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Compile(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetUniform(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SetTexture(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Delete(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_SHADERLIMPL_H */
