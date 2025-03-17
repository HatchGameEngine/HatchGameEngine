#ifndef ENGINE_FILESYSTEM_VFS_FILESYSTEMVFS_H
#define ENGINE_FILESYSTEM_VFS_FILESYSTEMVFS_H

#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

class FileSystemVFS : public VirtualFileSystem {
private:
	char* ParentPath = nullptr;
	VFSEntryMap Cache;

public:
	FileSystemVFS(const char *mountPoint, Uint16 flags) : VirtualFileSystem(mountPoint, flags) {};
	virtual ~FileSystemVFS();

	bool Open(const char* path);

	virtual bool HasFile(const char* filename);
	virtual VFSEntry* FindFile(const char* filename);
	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_FILESYSTEMVFS_H */
