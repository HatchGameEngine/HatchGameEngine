#ifndef ENGINE_BYTECODE_TYPEIMPL_STRINGIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_STRINGIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

class ScriptManager;

class StringImpl : public TypeImpl {
public:
	StringImpl(ScriptManager* manager);

	Obj* New(char* chars, size_t length);
	static void Dispose(Obj* object);

#ifdef HSL_VM
	static bool VM_ElementGet(Obj* object, VMValue at, VMValue* result, VMThread* thread);
	static bool VM_ElementSet(Obj* object, VMValue at, VMValue value, VMThread* thread);
#endif
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_STRINGIMPL_H */
