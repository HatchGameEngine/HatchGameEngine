#include <Engine/Filesystem/File.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/FileStream.h>

#if WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#if MACOSX || ANDROID
#include <Engine/Includes/StandardSDL2.h>
#endif

bool File::Exists(const char* path, bool allowURLs) {
	if (path == nullptr) {
		return false;
	}

	Stream* stream = FileStream::New(path, FileStream::READ_ACCESS, allowURLs);
	if (stream) {
		stream->Close();
		return true;
	}

	return false;
}

bool File::Exists(const char* path) {
	return Exists(path, false);
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
