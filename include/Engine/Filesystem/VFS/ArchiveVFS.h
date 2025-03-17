#ifndef ENGINE_FILESYSTEM_VFS_ARCHIVEVFS_H
#define ENGINE_FILESYSTEM_VFS_ARCHIVEVFS_H

#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

class ArchiveVFS : public VirtualFileSystem {
protected:
	VFSEntryMap Entries;
	size_t NumEntries = 0;

public:
	ArchiveVFS(const char *mountPoint, Uint16 flags) : VirtualFileSystem(mountPoint, flags) {};
	virtual ~ArchiveVFS();

	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual bool HasFile(const char* filename);
	virtual VFSEntry* FindFile(const char* filename);
	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_ARCHIVEVFS_H */
