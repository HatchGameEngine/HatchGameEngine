#ifndef ENGINE_FILESYSTEM_VFS_MEMORYCACHE_H
#define ENGINE_FILESYSTEM_VFS_MEMORYCACHE_H

#include <Engine/Filesystem/VFS/VirtualFileSystem.h>
#include <Engine/IO/Stream.h>

namespace MemoryCache {
//private:
	extern VirtualFileSystem* CacheVFS;

//public:
	extern bool Using;

	Stream* OpenStream(const char* filename, Uint32 access);
	bool Init();
	void Dispose();
};

#endif /* ENGINE_FILESYSTEM_VFS_MEMORYCACHE_H */
