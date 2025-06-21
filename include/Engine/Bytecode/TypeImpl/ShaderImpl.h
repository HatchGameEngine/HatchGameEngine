#ifndef ENGINE_BYTECODE_TYPEIMPL_SHADERLIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_SHADERLIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class ShaderImpl {
public:
	static ObjClass* Class;

	static void Init();
	static Obj* VM_New(void);

	static VMValue VM_AddProgram(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_AddTextureUnit(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Compile(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Delete(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_SHADERLIMPL_H */
