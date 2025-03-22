#ifndef ENGINE_FILESYSTEM_VFS_HATCHVFS_H
#define ENGINE_FILESYSTEM_VFS_HATCHVFS_H

#include <Engine/Filesystem/VFS/ArchiveVFS.h>

class HatchVFS : public ArchiveVFS {
private:
	Stream* StreamPtr = nullptr;

	static void CryptoXOR(Uint8* data, size_t size, Uint32 filenameHash, bool decrypt);

protected:
	Stream* OpenMemStreamForEntry(VFSEntry* entry);

public:
	HatchVFS(Uint16 flags) : ArchiveVFS(flags) {};
	virtual ~HatchVFS();

	bool Open(Stream* stream);

	virtual std::string TransformFilename(const char* filename);
	virtual bool SupportsCompression();
	virtual bool SupportsEncryption();
	virtual bool ReadEntryData(VFSEntry* entry, Uint8* memory, size_t memSize);
	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual VFSEnumeration EnumerateFiles(const char* path);
	virtual bool Flush();
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_HATCHVFS_H */
