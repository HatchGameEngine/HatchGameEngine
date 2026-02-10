#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/File.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/StandardIOStream.h>

#if WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

Stream* File::Open(const char* filename, Uint32 access) {
	// TODO: On Android, retry with SDLStream.
	Uint32 streamAccess;
	switch (access) {
	case File::READ_ACCESS:
		streamAccess = StandardIOStream::READ_ACCESS;
		break;
	case File::WRITE_ACCESS:
		streamAccess = StandardIOStream::WRITE_ACCESS;
		break;
	case File::APPEND_ACCESS:
		streamAccess = StandardIOStream::APPEND_ACCESS;
		break;
	default:
		return nullptr;
	}

	return StandardIOStream::New(filename, streamAccess);
}

// Do not expose to HSL.
bool File::Exists(const char* path) {
	Stream* stream = Open(path, READ_ACCESS);
	if (stream) {
		stream->Close();
		return true;
	}

	return false;
}

bool File::ProtectedExists(const char* path, bool allowURLs) {
	if (path == nullptr) {
		return false;
	}

	return FileStream::Exists(path, allowURLs);
}

size_t File::ReadAllBytes(const char* path, char** out, bool allowURLs) {
	FileStream* stream = FileStream::New(path, FileStream::READ_ACCESS, allowURLs);
	if (stream) {
		size_t size = stream->Length();
		*out = (char*)Memory::Malloc(size + 1);
		if (!*out) {
			return 0;
		}
		(*out)[size] = 0;
		stream->ReadBytes(*out, size);
		stream->Close();
		return size;
	}
	return 0;
}

bool File::WriteAllBytes(const char* path, const char* bytes, size_t len, bool allowURLs) {
	if (!path) {
		return false;
	}
	if (!*path) {
		return false;
	}
	if (!bytes) {
		return false;
	}

	FileStream* stream = FileStream::New(path, FileStream::WRITE_ACCESS, allowURLs);
	if (stream) {
		stream->WriteBytes((char*)bytes, len);
		stream->Close();
		return true;
	}
	return false;
}
