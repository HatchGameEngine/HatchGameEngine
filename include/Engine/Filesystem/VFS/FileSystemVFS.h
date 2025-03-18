#ifndef ENGINE_FILESYSTEM_VFS_FILESYSTEMVFS_H
#define ENGINE_FILESYSTEM_VFS_FILESYSTEMVFS_H

#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

class FileSystemVFS : public VirtualFileSystem {
private:
	std::string ParentPath = "";
	VFSEntryMap Cache;

	bool GetPath(const char* filename, char* path, size_t pathSize);

public:
	FileSystemVFS(const char *mountPoint, Uint16 flags) : VirtualFileSystem(mountPoint, flags) {};
	virtual ~FileSystemVFS();

	bool Open(const char* path);

	virtual bool HasFile(const char* filename);
	virtual VFSEntry* FindFile(const char* filename);
	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual Stream* OpenReadStream(const char* filename);
	virtual Stream* OpenWriteStream(const char* filename);
	virtual bool CloseStream(Stream*);
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_FILESYSTEMVFS_H */
