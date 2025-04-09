#ifndef ENGINE_BYTECODE_SOURCEFILEMAP_H
#define ENGINE_BYTECODE_SOURCEFILEMAP_H

#include <Engine/Bytecode/Compiler.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Includes/HashMap.h>

namespace SourceFileMap {
//private:
	void AddToList(Compiler* compiler, Uint32 filenameHash);

//public:
	extern bool Initialized;
	extern HashMap<Uint32>* Checksums;
	extern HashMap<vector<Uint32>*>* ClassMap;
	extern Uint32 DirectoryChecksum;
	extern Uint32 Magic;
	extern bool DoLogging;

	void CheckInit();
	void CheckForUpdate();
	void Dispose();
};

#endif /* ENGINE_BYTECODE_SOURCEFILEMAP_H */
