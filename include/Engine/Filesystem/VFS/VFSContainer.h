#ifndef ENGINE_FILESYSTEM_VFS_VFSCONTAINER_H
#define ENGINE_FILESYSTEM_VFS_VFSCONTAINER_H

#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

#define DEFAULT_MOUNT_POINT ""

enum VFSMountStatus {
	NOT_FOUND = -3,
	COULD_NOT_MOUNT = -2,
	COULD_NOT_UNMOUNT = -1,
	UNMOUNTED = 0,
	MOUNTED = 1
};

class VFSContainer {
private:
	std::deque<VirtualFileSystem*> LoadedVFS;

	void Dispose();

public:
	~VFSContainer();

	VFSMountStatus Mount(const char* filename, const char* mountPoint, VFSType type, Uint16 flags);
	int NumMounted();

	bool LoadFile(const char* filename, Uint8** out, size_t* size);
	bool FileExists(const char* filename);
};

#endif /* ENGINE_FILESYSTEM_VFS_VFSCONTAINER_H */
