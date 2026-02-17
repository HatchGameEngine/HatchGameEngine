#ifndef ENGINE_BYTECODE_SOURCEFILEMAP_H
#define ENGINE_BYTECODE_SOURCEFILEMAP_H

#include <Engine/Includes/HashMap.h>

#ifdef HSL_COMPILER
#include <Engine/Bytecode/Compiler.h>
#endif

#define SCRIPTS_DIRECTORY_NAME "Scripts"

class SourceFileMap {
#ifdef HSL_COMPILER
private:
	static void AddToList(Compiler* compiler, Uint32 filenameHash);
	static void HandleCompileError(const char* error);
#endif

public:
	static bool Initialized;
	static bool AllowCompilation;
	static HashMap<Uint32>* Checksums;
	static HashMap<vector<Uint32>*>* ClassMap;
	static Uint32 DirectoryChecksum;
	static Uint32 Magic;
	static bool DoLogging;

	static void CheckInit();
	static bool CheckForUpdate();
	static void Dispose();
};

#endif /* ENGINE_BYTECODE_SOURCEFILEMAP_H */
