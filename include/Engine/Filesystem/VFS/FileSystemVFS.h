#ifndef ENGINE_FILESYSTEM_VFS_FILESYSTEMVFS_H
#define ENGINE_FILESYSTEM_VFS_FILESYSTEMVFS_H

#include <Engine/Filesystem/VFS/VFSProvider.h>

class FileSystemVFS : public VFSProvider {
private:
	std::string ParentPath = "";
	VFSEntryMap Cache;

	bool GetPath(const char* filename, char* path, size_t pathSize);
	VFSEntry* CacheEntry(const char* filename, Stream* stream);
	VFSEntry* GetCachedEntry(const char* filename);
	void RemoveFromCache(const char* filename);

protected:
	virtual VFSEntry* FindFile(const char* filename);

public:
	FileSystemVFS(Uint16 flags) : VFSProvider(flags) {};
	virtual ~FileSystemVFS();

	bool Open(const char* path);

	virtual bool HasFile(const char* filename);
	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual VFSEnumeration EnumerateFiles(const char* path);
	virtual Stream* OpenReadStream(const char* filename);
	virtual Stream* OpenWriteStream(const char* filename);
	virtual Stream* OpenAppendStream(const char* filename);
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_FILESYSTEMVFS_H */
