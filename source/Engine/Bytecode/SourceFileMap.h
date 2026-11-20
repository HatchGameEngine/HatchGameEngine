#ifndef ENGINE_BYTECODE_SOURCEFILEMAP_H
#define ENGINE_BYTECODE_SOURCEFILEMAP_H

#include <Engine/Bytecode/Compiler.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Includes/HashMap.h>

#define SCRIPTS_DIRECTORY_NAME "Scripts"

class SourceFileMap {
private:
	static bool Loaded;
	static HashMap<Uint32>* Checksums;

	static void ReadFileMap();
	static void AddToList(Compiler* compiler, Uint32 filenameHash);
	static void HandleCompileError(const char* error);
	static void Load();

public:
	static bool Initialized;
	static char Path[MAX_PATH_LENGTH];
	static bool AllowCompilation;
	static HashMap<vector<Uint32>*>* ClassMap;
	static Uint32 DirectoryChecksum;
	static Uint32 Magic;

	static void Init();
	static bool CheckForUpdate();
	static void Dispose();
};

#endif /* ENGINE_BYTECODE_SOURCEFILEMAP_H */
