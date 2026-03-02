#ifndef ENGINE_BYTECODE_TYPEIMPL_SHADERIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_SHADERIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

class ScriptManager;

#define IS_SHADER(value) IsNativeInstance(value, CLASS_SHADER)
#define AS_SHADER(value) ((ObjShader*)AS_OBJECT(value))

class ShaderImpl : public TypeImpl {
public:
	ShaderImpl(ScriptManager* manager);

	static Obj* New(VMThread* thread);
	ObjShader* New(void* shaderPtr);
	static void Dispose(Obj* object);

	static bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, VMThread* thread);

	static VMValue VM_HasStage(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_CanCompile(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_IsValid(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_AddStage(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_AddStageFromString(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_AssignTextureUnit(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_GetTextureUnit(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_HasUniform(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_GetUniformType(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_Compile(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_SetUniform(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_SetTexture(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_Delete(int argCount, VMValue* args, VMThread* thread);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_SHADERIMPL_H */
