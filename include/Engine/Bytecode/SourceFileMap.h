#ifndef ENGINE_BYTECODE_SOURCEFILEMAP_H
#define ENGINE_BYTECODE_SOURCEFILEMAP_H

#include <Engine/Bytecode/Compiler.h>
#include <Engine/Filesystem/VFS/VirtualFileSystem.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/IO/Stream.h>
#include <Engine/Includes/HashMap.h>

#define SCRIPTS_DIRECTORY_NAME "Scripts"

class SourceFileMap {
private:
	static VirtualFileSystem* ScriptsVFS;
	static void AddToList(Compiler* compiler, Uint32 filenameHash);
	static void HandleCompileError(const char* error);

public:
	static bool Initialized;
	static bool AllowCompilation;
	static HashMap<Uint32>* Checksums;
	static HashMap<vector<Uint32>*>* ClassMap;
	static Uint32 DirectoryChecksum;
	static Uint32 Magic;
	static bool DoLogging;

	static void Initialize();
	static bool ReadClassMap(Stream* stream);
	static bool MountScriptsVFS(const char* scriptsPath);
	static bool CheckForUpdate();
	static void Dispose();
};

#endif /* ENGINE_BYTECODE_SOURCEFILEMAP_H */
