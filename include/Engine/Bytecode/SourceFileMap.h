#ifndef ENGINE_BYTECODE_SOURCEFILEMAP_H
#define ENGINE_BYTECODE_SOURCEFILEMAP_H

#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Includes/HashMap.h>

class SourceFileMap {
public:
	static bool Initialized;
	static HashMap<Uint32>* Checksums;
	static HashMap<vector<Uint32>*>* ClassMap;
	static Uint32 DirectoryChecksum;
	static Uint32 Magic;
	static bool DoLogging;

	static void CheckInit();
	static void CheckForUpdate();
	static void Dispose();
};

#endif /* ENGINE_BYTECODE_SOURCEFILEMAP_H */
