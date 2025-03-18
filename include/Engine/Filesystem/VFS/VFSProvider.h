#ifndef ENGINE_FILESYSTEM_VFS_VFSPROVIDER_H
#define ENGINE_FILESYSTEM_VFS_VFSPROVIDER_H

#include <Engine/Filesystem/VFS/VFSEntry.h>

#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>

enum VFSType {
	FILESYSTEM,
	HATCH,
	MEMORY
};

#define VFS_READABLE 1
#define VFS_WRITABLE 2

struct VFSOpenStream {
	VFSEntry* EntryPtr;
	Stream* StreamPtr;
};

class VFSProvider {
private:
	Uint16 Flags;
	std::string MountPoint;

protected:
	bool Opened;
	std::vector<VFSOpenStream> OpenStreams;

	void AddOpenStream(VFSEntry* entry, Stream* stream);

	virtual VFSEntry* FindFile(const char* filename);

public:
	VFSProvider(const char *mountPoint, Uint16 flags);
	virtual ~VFSProvider();

	bool IsOpen();
	bool IsReadable();
	bool IsWritable();

	const char* GetMountPoint();

	virtual void TransformFilename(const char* filename, char* dest, size_t destSize);
	virtual bool HasFile(const char* filename);
	virtual bool ReadFile(const char* filename, Uint8** out, size_t* size);
	virtual bool PutFile(const char* filename, VFSEntry* entry);
	virtual bool EraseFile(const char* filename);
	virtual Stream* OpenReadStream(const char* filename);
	virtual Stream* OpenWriteStream(const char* filename);
	virtual Stream* OpenAppendStream(const char* filename);
	virtual bool CloseStream(Stream*);
	virtual void Close();
};

#endif /* ENGINE_FILESYSTEM_VFS_VFSPROVIDER_H */
