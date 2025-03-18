#ifndef ENGINE_RESOURCETYPES_RESOURCEMANAGER_H
#define ENGINE_RESOURCETYPES_RESOURCEMANAGER_H

#include <Engine/Filesystem/VFS/VirtualFileSystem.h>
#include <Engine/Includes/Standard.h>

class ResourceManager {
public:
	static bool UsingDataFolder;
	static bool UsingModPack;

	static bool Init(const char* filename);
	static bool Mount(const char* name, const char* filename, const char *mountPoint, VFSType type,
		Uint16 flags);
	static bool LoadResource(const char* filename, Uint8** out, size_t* size);
	static bool ResourceExists(const char* filename);
	static void Dispose();
};

#endif /* ENGINE_RESOURCETYPES_RESOURCEMANAGER_H */
