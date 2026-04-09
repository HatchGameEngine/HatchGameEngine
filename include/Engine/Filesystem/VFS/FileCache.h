#ifndef ENGINE_FILESYSTEM_VFS_FILECACHE_H
#define ENGINE_FILESYSTEM_VFS_FILECACHE_H

#include <Engine/Filesystem/VFS/VirtualFileSystem.h>
#include <Engine/IO/Stream.h>

class FileCache {
private:
	static VirtualFileSystem* CacheVFS;

public:
	static bool Using;
	static bool InMemory;

	static VirtualFileSystem* GetVFS();
	static Stream* OpenStream(const char* filename, Uint32 access);
	static bool Exists(const char* filename);
	static bool Init(bool useMemoryFileCache);
	static void Dispose();
};

#endif /* ENGINE_FILESYSTEM_VFS_FILECACHE_H */
