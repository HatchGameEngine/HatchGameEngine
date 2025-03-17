#ifndef ENGINE_FILESYSTEM_VFS_HATCHVFS_H
#define ENGINE_FILESYSTEM_VFS_HATCHVFS_H

#include <Engine/Filesystem/VFS/ArchiveVFS.h>

class HatchVFS : public ArchiveVFS {
private:
	Stream* StreamPtr = nullptr;

	static void CryptoXOR(Uint8* data, size_t size, Uint32 filenameHash, bool decrypt);

public:
	HatchVFS(const char *mountPoint, Uint16 flags) : ArchiveVFS(mountPoint, flags) {};
	virtual ~HatchVFS();

	bool Open(Stream* stream);

	virtual void TransformFilename(const char* filename, char* dest, size_t destSize);
	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual bool Flush();
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_HATCHVFS_H */
