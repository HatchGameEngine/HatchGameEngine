#ifndef ENGINE_RESOURCETYPES_RESOURCEMANAGER_H
#define ENGINE_RESOURCETYPES_RESOURCEMANAGER_H

#include <Engine/Filesystem/VFS/VFSProvider.h>
#include <Engine/Filesystem/VFS/VirtualFileSystem.h>
#include <Engine/Includes/Standard.h>

namespace ResourceManager {
//public:
	extern bool UsingDataFolder;
	extern bool UsingModPack;

	bool Init(const char* filename);
	bool Mount(const char* name,
		const char* filename,
		const char* mountPoint,
		VFSType type,
		Uint16 flags);
	bool Unmount(const char* name);
	VirtualFileSystem* GetVFS();
	VFSProvider* GetMainResource();
	void SetMainResourceWritable(bool writable);
	bool LoadResource(const char* filename, Uint8** out, size_t* size);
	bool ResourceExists(const char* filename);
	void Dispose();
};

#endif /* ENGINE_RESOURCETYPES_RESOURCEMANAGER_H */
