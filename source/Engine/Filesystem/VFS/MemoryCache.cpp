#include <Engine/Filesystem/VFS/MemoryCache.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/VirtualFileStream.h>

bool MemoryCache::Using = false;

VirtualFileSystem* MemoryCache::CacheVFS = nullptr;

bool MemoryCache::Init() {
	CacheVFS = new VirtualFileSystem();
	if (CacheVFS == nullptr) {
		return false;
	}

	Using = true;

	if (!CacheVFS->Mount("memorycache",
		    nullptr,
		    nullptr,
		    VFSType::MEMORY,
		    VFS_READABLE | VFS_WRITABLE)) {
		Dispose();
	}

	return Using;
}

Stream* MemoryCache::OpenStream(const char* filename, Uint32 access) {
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

void MemoryCache::Dispose() {
	if (CacheVFS) {
		delete CacheVFS;

		CacheVFS = nullptr;
	}

	Using = false;
}
