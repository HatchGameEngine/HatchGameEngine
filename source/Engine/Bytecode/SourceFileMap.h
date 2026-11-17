#ifndef ENGINE_BYTECODE_SOURCEFILEMAP_H
#define ENGINE_BYTECODE_SOURCEFILEMAP_H

#include <Engine/Bytecode/Compiler.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Includes/HashMap.h>

#define SCRIPTS_DIRECTORY_NAME "Scripts"

struct ModInfo;

class SourceFileMap {
private:
	static bool Loaded;
	static HashMap<Uint32>* Checksums;
	static vector<Uint32> ModFilenameHashes;

	static void ReadFileMap();
	static void WriteFileMap();
	static bool ReadClassMap(Stream* stream, HashMap<vector<Uint32>*>* classMap);
	static void WriteClassMap(Stream* stream, HashMap<vector<Uint32>*>* classMap);
	static void ClearClassMap(HashMap<vector<Uint32>*>* classMap);
	static char* ReadSource(const char* filename);
	static bool CompileScript(const char* scriptFilename, char* source, Uint32 filenameHash, HashMap<vector<Uint32>*>* classMap, Stream* output);
	static void AddToList(HashMap<vector<Uint32>*>* classMap, Compiler* compiler, Uint32 filenameHash);
	static void HandleCompileError(const char* error);
	static void Load();
	static bool CheckForBaseUpdate();
	static int GetModBytecodeVersion(ModInfo* mod, Uint32 filenameHash);
	static void LoadModClassMap(ModInfo* mod, HashMap<vector<Uint32>*>* modClassMap);
	static bool CheckForModUpdate(ModInfo* mod, HashMap<vector<Uint32>*>* modClassMap);
	static void AddModScripts(HashMap<vector<Uint32>*>* modClassMap);
	static void RemoveModScripts();
	static bool IsModFilename(Uint32 filenameHash);

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
