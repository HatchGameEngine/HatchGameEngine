#ifndef ENGINE_BYTECODE_SOURCEFILEMAP_H
#define ENGINE_BYTECODE_SOURCEFILEMAP_H

#include <Engine/Bytecode/Compiler.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Includes/HashMap.h>

#define SCRIPTS_DIRECTORY_NAME "Scripts"

class SourceFileMap {
private:
	static void AddToList(Compiler* compiler, Uint32 filenameHash);

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
