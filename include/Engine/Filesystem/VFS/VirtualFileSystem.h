#ifndef ENGINE_FILESYSTEM_VFS_VIRTUALFILESYSTEM_H
#define ENGINE_FILESYSTEM_VFS_VIRTUALFILESYSTEM_H

#include <Engine/Filesystem/VFS/VFSProvider.h>
#include <Engine/IO/Stream.h>

#define DEFAULT_MOUNT_POINT ""

enum VFSMountStatus {
	NOT_FOUND = CHAR_MIN,
	ALREADY_EXISTS,
	COULD_NOT_MOUNT,
	COULD_NOT_UNMOUNT,

	UNMOUNTED = 0,
	MOUNTED
};

struct VFSMount {
	std::string Name;
	std::string MountPoint;
	VFSProvider* VFSPtr;
};

class VirtualFileSystem {
private:
	std::deque<VFSMount> LoadedVFS;

	void Dispose();

public:
	~VirtualFileSystem();

	size_t GetIndex(const char* name);
	VFSProvider* Get(const char* name);
	VFSProvider* Get(size_t index);
	VFSMountStatus Mount(const char* name,
		const char* filename,
		const char* mountPoint,
		VFSType type,
		Uint16 flags);
	VFSMountStatus Unmount(const char* name);
	int NumMounted();
	const char* GetFilename(VFSMount mount, const char* filename);

	bool LoadFile(const char* filename, Uint8** out, size_t* size);
	bool FileExists(const char* filename);

	Stream* OpenReadStream(const char* filename);
	Stream* OpenWriteStream(const char* filename);
	Stream* OpenAppendStream(const char* filename);
	bool CloseStream(Stream* stream);
};

#endif /* ENGINE_FILESYSTEM_VFS_VIRTUALFILESYSTEM_H */
