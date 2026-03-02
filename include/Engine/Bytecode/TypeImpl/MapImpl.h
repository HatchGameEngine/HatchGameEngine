#ifndef ENGINE_BYTECODE_TYPEIMPL_MAPIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_MAPIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

class ScriptManager;

class MapImpl : public TypeImpl {
public:
	MapImpl(ScriptManager* manager);

	Obj* New();
	static void Dispose(Obj* object);

#ifdef HSL_VM
	static VMValue VM_GetKeys(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_RemoveKey(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_Iterate(int argCount, VMValue* args, VMThread* thread);
	static VMValue VM_IteratorValue(int argCount, VMValue* args, VMThread* thread);
#endif
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_MAPIMPL_H */
