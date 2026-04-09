#include <Engine/Filesystem/Path.h>
#include <Engine/Filesystem/VFS/FileCache.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/VirtualFileStream.h>

bool FileCache::Using = false;
bool FileCache::InMemory = false;

VirtualFileSystem* FileCache::CacheVFS = nullptr;

bool FileCache::Init(bool useMemoryFileCache) {
	if (CacheVFS) {
		return true;
	}

	CacheVFS = new VirtualFileSystem();
	if (CacheVFS == nullptr) {
		return false;
	}

	Using = true;
	InMemory = useMemoryFileCache;

	if (useMemoryFileCache) {
		if (!CacheVFS->Mount("memorycache",
			    nullptr,
			    nullptr,
			    VFSType::MEMORY,
			    VFS_READABLE | VFS_WRITABLE)) {
			Dispose();
		}
	}
	else {
		std::string path = Path::GetCachePath();
		if (!CacheVFS->Mount("devicecache",
			    path.c_str(),
			    nullptr,
			    VFSType::FILESYSTEM,
			    VFS_READABLE | VFS_WRITABLE)) {
			Dispose();
		}
	}

	return Using;
}

VirtualFileSystem* FileCache::GetVFS() {
	return CacheVFS;
}

Stream* FileCache::OpenStream(const char* filename, Uint32 access) {
	if (!CacheVFS) {
		return nullptr;
	}

	Stream* stream = nullptr;

	switch (access) {
	case FileStream::READ_ACCESS:
		stream = VirtualFileStream::New(CacheVFS, filename, VirtualFileStream::READ_ACCESS);
		break;
	case FileStream::WRITE_ACCESS:
		stream =
			VirtualFileStream::New(CacheVFS, filename, VirtualFileStream::WRITE_ACCESS);
		break;
	case FileStream::APPEND_ACCESS:
		stream = VirtualFileStream::New(
			CacheVFS, filename, VirtualFileStream::APPEND_ACCESS);
		break;
	default:
		break;
	}

	return stream;
}

bool FileCache::Exists(const char* filename) {
	if (CacheVFS) {
		return CacheVFS->FileExists(filename);
	}

	return false;
}

void FileCache::Dispose() {
	if (CacheVFS) {
		delete CacheVFS;

		CacheVFS = nullptr;
	}

	Using = false;
	InMemory = false;
}
