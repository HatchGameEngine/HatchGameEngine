#ifndef ENGINE_BYTECODE_TYPEIMPL_TYPEIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_TYPEIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define GET_STRING_HASH(str) \
	Hash_##str = Murmur::EncryptString(#str)

#define DEF_CLASS_NATIVE(className, funcName) \
	ScriptManager::DefineNative(Class, #funcName, className##_##funcName)

class TypeImpl {
public:
	static void Init();

	static void RegisterClass(ObjClass* klass);
	static void ExposeClass(const char* name, ObjClass* klass);

	static void DefinePrintableName(ObjClass* klass, const char* name);
	static const char* GetPrintableName(ObjClass* klass);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_TYPEIMPL_H */
