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

public:
	ArchiveVFS(const char *mountPoint, Uint16 flags) : VFSProvider(mountPoint, flags) {};
	virtual ~ArchiveVFS();

	virtual bool HasFile(const char* filename);
	virtual VFSEntry* FindFile(const char* filename);
	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual bool ReadEntryData(VFSEntry* entry, Uint8* memory);
	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual bool Flush();
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_ARCHIVEVFS_H */
