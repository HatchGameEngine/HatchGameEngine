#ifndef ENGINE_FILESYSTEM_VFS_VIRTUALFILESYSTEM_H
#define ENGINE_FILESYSTEM_VFS_VIRTUALFILESYSTEM_H

#include <Engine/Filesystem/VFS/VFSProvider.h>
#include <Engine/IO/Stream.h>

#define DEFAULT_MOUNT_POINT ""

enum VFSMountStatus {
	NOT_FOUND = -3,
	COULD_NOT_MOUNT = -2,
	COULD_NOT_UNMOUNT = -1,
	UNMOUNTED = 0,
	MOUNTED = 1
};

class VirtualFileSystem {
private:
	std::deque<VFSProvider*> LoadedVFS;

	void Dispose();

public:
	~VirtualFileSystem();

	VFSMountStatus Mount(const char* filename, const char* mountPoint, VFSType type, Uint16 flags);
	int NumMounted();

	bool LoadFile(const char* filename, Uint8** out, size_t* size);
	bool FileExists(const char* filename);

	Stream* OpenReadStream(const char* filename);
	Stream* OpenWriteStream(const char* filename);
	bool CloseStream(Stream* stream);
};

#endif /* ENGINE_FILESYSTEM_VFS_VIRTUALFILESYSTEM_H */
