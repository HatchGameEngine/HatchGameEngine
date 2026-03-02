#ifndef ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

class ScriptManager;

class FunctionImpl : public TypeImpl {
public:
	FunctionImpl(ScriptManager* manager);

	Obj* New();

#ifdef HSL_VM
	static VMValue VM_Bind(int argCount, VMValue* args, VMThread* thread);
#endif
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H */
