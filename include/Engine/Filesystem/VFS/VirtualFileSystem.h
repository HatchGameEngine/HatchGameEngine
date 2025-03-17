#ifndef ENGINE_FILESYSTEM_VFS_VIRTUALFILESYSTEM_H
#define ENGINE_FILESYSTEM_VFS_VIRTUALFILESYSTEM_H

#include <Engine/Filesystem/VFS/VFSEntry.h>

#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>

enum VFSType {
	FILESYSTEM,
	HATCH
};

#define VFS_READABLE 1
#define VFS_WRITABLE 2

typedef std::unordered_map<std::string, VFSEntry*> VFSEntryMap;

class VirtualFileSystem {
private:
	Uint16 Flags;

protected:
	bool Opened;

public:
	VirtualFileSystem(Uint16 flags);
	virtual ~VirtualFileSystem();

	bool IsOpen();
	bool IsReadable();
	bool IsWritable();

	virtual void TransformFilename(const char* filename, char* dest, size_t destSize);
	virtual bool HasFile(const char* filename);
	virtual VFSEntry* FindFile(const char* filename);
	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_VIRTUALFILESYSTEM_H */
