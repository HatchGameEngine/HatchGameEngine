#ifndef ENGINE_FILESYSTEM_VFS_MEMORYCACHE_H
#define ENGINE_FILESYSTEM_VFS_MEMORYCACHE_H

#include <Engine/Filesystem/VFS/VirtualFileSystem.h>
#include <Engine/IO/Stream.h>

class MemoryCache {
private:
	static VirtualFileSystem* CacheVFS;

public:
	static bool Using;

	static Stream* OpenStream(const char* filename, Uint32 access);
	static bool Init();
	static void Dispose();
};

#endif /* ENGINE_FILESYSTEM_VFS_MEMORYCACHE_H */
