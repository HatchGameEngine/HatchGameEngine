#ifndef ENGINE_FILESYSTEM_VFS_ARCHIVEVFS_H
#define ENGINE_FILESYSTEM_VFS_ARCHIVEVFS_H

#include <Engine/Filesystem/VFS/VFSProvider.h>
#include <Engine/IO/Stream.h>

class ArchiveVFS : public VFSProvider {
protected:
	VFSEntryMap Entries;
	std::vector<std::string> EntryNames;
	size_t NumEntries = 0;

	bool NeedsRepacking = false;

	bool AddEntry(VFSEntry* entry);
	virtual Stream* OpenMemStreamForEntry(VFSEntry &entry);

protected:
	virtual VFSEntry* FindFile(const char* filename);

public:
	ArchiveVFS(Uint16 flags) : VFSProvider(flags) {};
	virtual ~ArchiveVFS();

	virtual bool HasFile(const char* filename);
	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual bool ReadEntryData(VFSEntry &entry, Uint8 *memory, size_t memSize);
	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual VFSEnumeration EnumerateFiles(const char* path);
	virtual Stream* OpenReadStream(const char* filename);
	virtual Stream* OpenWriteStream(const char* filename);
	virtual Stream* OpenAppendStream(const char* filename);
	virtual bool CloseStream(Stream* stream);
	virtual bool Flush();
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_ARCHIVEVFS_H */
