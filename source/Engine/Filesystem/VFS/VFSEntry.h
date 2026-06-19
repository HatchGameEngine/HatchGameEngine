#ifndef ENGINE_FILESYSTEM_VFS_VFSENTRY_H
#define ENGINE_FILESYSTEM_VFS_VFSENTRY_H

#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>

#define VFSE_COMPRESSED 1
#define VFSE_ENCRYPTED 2

class VFSEntry {
public:
	std::string Name;
	Uint64 Offset = 0;
	Uint64 Size = 0;
	Uint8 Flags = 0;
	Uint8 FileFlags = 0;
	Uint64 CompressedSize = 0;
	Uint8* CachedData = nullptr;
	Uint8* CachedDataInFile = nullptr;

	static VFSEntry* FromStream(const char* filename, Stream* stream);

	void DeleteCache() {
		if (CachedDataInFile != CachedData) {
			Memory::Free(CachedDataInFile);
		}

		Memory::Free(CachedData);

		CachedData = nullptr;
		CachedDataInFile = nullptr;
	}

	~VFSEntry() { DeleteCache(); }
};

typedef std::unordered_map<std::string, VFSEntry*> VFSEntryMap;

#endif /* ENGINE_FILESYSTEM_VFS_VFSENTRY_H */
