#ifndef ENGINE_FILESYSTEM_VFS_HATCHVFS_H
#define ENGINE_FILESYSTEM_VFS_HATCHVFS_H

#include <Engine/Filesystem/VFS/ArchiveVFS.h>

class HatchVFS : public ArchiveVFS {
private:
	Stream* StreamPtr = nullptr;

public:
	HatchVFS(const char *mountPoint, Uint16 flags) : ArchiveVFS(mountPoint, flags) {};
	virtual ~HatchVFS();

	bool Open(Stream* stream);

	virtual void TransformFilename(const char* filename, char* dest, size_t destSize);
	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_HATCHVFS_H */
