#ifndef ENGINE_FILESYSTEM_VFS_MEMORYVFS_H
#define ENGINE_FILESYSTEM_VFS_MEMORYVFS_H

#include <Engine/Filesystem/VFS/ArchiveVFS.h>

class MemoryVFS : public ArchiveVFS {
public:
	MemoryVFS(const char* name, const char *mountPoint, Uint16 flags)
		: ArchiveVFS(name, mountPoint, flags) {};
	virtual ~MemoryVFS();

	bool Open();

	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual bool ReadEntryData(VFSEntry* entry, Uint8* memory, size_t memSize);
	virtual bool Flush();
};

#endif /* ENGINE_FILESYSTEM_VFS_MEMORYVFS_H */
