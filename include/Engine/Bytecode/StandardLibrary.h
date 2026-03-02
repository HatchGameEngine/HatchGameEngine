#ifndef ENGINE_BYTECODE_STANDARDLIBRARY_H
#define ENGINE_BYTECODE_STANDARDLIBRARY_H

#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/Types.h>

class StandardLibrary {
public:
	static void Init();
	static void Link(ScriptManager* manager);
	static void Dispose();
};

#endif /* ENGINE_BYTECODE_STANDARDLIBRARY_H */
