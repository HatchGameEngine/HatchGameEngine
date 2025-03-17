#ifndef ENGINE_FILESYSTEM_VFS_VFSENTRY_H
#define ENGINE_FILESYSTEM_VFS_VFSENTRY_H

#include <Engine/Includes/Standard.h>

#define VFSE_ENCRYPTED 1

class VFSEntry {
public:
	std::string Name;
	Uint64 Offset = 0;
	Uint64 Size = 0;
	Uint32 Flags = 0;
	Uint64 CompressedSize = 0;
};

#endif /* ENGINE_FILESYSTEM_VFS_VFSENTRY_H */
