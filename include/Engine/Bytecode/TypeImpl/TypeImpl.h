#ifndef ENGINE_BYTECODE_TYPEIMPL_TYPEIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_TYPEIMPL_H

class ScriptManager;

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

class TypeImpl {
private:
	static HashMap<const char*>* PrintableClassNames;

public:
	ScriptManager* Manager;
	ObjClass* Class;

	static void Init();
	static void Dispose();

	static void RegisterClass(ScriptManager* manager, ObjClass* klass);
	static void ExposeClass(ScriptManager* manager, const char* name, ObjClass* klass);

	static void DefinePrintableName(ObjClass* klass, const char* name);
	static const char* GetPrintableName(ObjClass* klass);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_TYPEIMPL_H */
