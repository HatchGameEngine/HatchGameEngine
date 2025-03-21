#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Filesystem/VFS/MemoryCache.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/StandardIOStream.h>
#include <Engine/Includes/StandardSDL2.h>

#ifdef MACOSX
extern "C" {
#include <Engine/Platforms/MacOS/Filesystem.h>
}

static void getAppName(char* buffer, int maxSize) {
	char* tmp = new char[maxSize];
	char* tmp2 = new char[maxSize];

	if (MacOS_GetSelfPath(tmp, maxSize)) {
		/* this shouldn't happen */
		delete[] tmp;
		delete[] tmp2;

		return;
	}

	memset(tmp2, 0, maxSize);

	int slashcount = 0;
	int end = 0;
	for (int i = maxSize - 1; i >= 0; --i) {
		slashcount += tmp[i] == '/' ? 1 : 0;

		if (slashcount >= 2) {
			end = i;
			break;
		}
	}

	if (slashcount < 2) {
		memset(buffer, 0, maxSize);

		delete[] tmp;
		delete[] tmp2;

		return;
	}

	int start = -1;
	for (int i = end - 1; i >= 0; --i) {
		if (tmp[i] == '/') {
			start = i + 1;

			break;
		}
	}

	if (start == -1) {
		memset(buffer, 0, maxSize);

		delete[] tmp;
		delete[] tmp2;

		return;
	}

	if (tmp[end - 1] != 'p' || tmp[end - 2] != 'p' || tmp[end - 3] != 'a' ||
		tmp[end - 4] != '.') {
		/* no .app, it's not packaged */
		memset(buffer, 0, maxSize);

		delete[] tmp;
		delete[] tmp2;

		return;
	}

	memcpy(buffer, (unsigned char*)tmp + start, (end - start) - 4);

	delete[] tmp;
	delete[] tmp2;
}
#endif

Stream* FileStream::OpenFile(const char* filename, Uint32 access, bool allowURLs) {
	Uint32 streamAccess = 0;
	switch (access) {
	case FileStream::READ_ACCESS:
		streamAccess = StandardIOStream::READ_ACCESS;
		break;
	case FileStream::WRITE_ACCESS:
		streamAccess = StandardIOStream::WRITE_ACCESS;
		break;
	case FileStream::APPEND_ACCESS:
		streamAccess = StandardIOStream::APPEND_ACCESS;
		break;
	}

	std::string resolvedPath = "";

	PathLocation location = PathLocation::DEFAULT;

	bool isPathValid = true;
	if (!Path::FromURL(filename, resolvedPath, location, access != FileStream::READ_ACCESS)) {
		isPathValid = false;
	}

	if (isPathValid && !allowURLs && location != PathLocation::DEFAULT) {
		isPathValid = false;
	}

	if (!isPathValid) {
		Log::Print(Log::LOG_ERROR, "Path \"%s\" is not valid!", filename);
		return nullptr;
	}

	const char* finalPath = resolvedPath.c_str();

	Stream* stream = nullptr;

	switch (location) {
	case PathLocation::CACHE:
		if (MemoryCache::Using) {
			// Use the original filename instead of the resolved one.
			std::string resolvedPath = Path::StripURL(filename);
			finalPath = resolvedPath.c_str();

			stream = MemoryCache::OpenStream(finalPath, access);
			break;
		}
		/* FALLTHRU */
	default:
		stream = StandardIOStream::New(finalPath, streamAccess);
		break;
	}

	return stream;
}

FileStream* FileStream::New(const char* filename, Uint32 access, bool allowURLs) {
	FileStream* stream = new (std::nothrow) FileStream;
	if (!stream) {
		return nullptr;
	}

	stream->StreamPtr = OpenFile(filename, access, allowURLs);

	if (!stream->StreamPtr) {
		goto FREE;
	}

	stream->Filename = std::string(filename);
	stream->CurrentAccess = access;
	stream->UseURLs = allowURLs;

	return stream;

FREE:
	delete stream;
	return nullptr;
}
FileStream* FileStream::New(const char* filename, Uint32 access) {
	return New(filename, access, false);
}

bool FileStream::Reopen(Uint32 newAccess) {
	if (CurrentAccess == newAccess) {
		return true;
	}

	Stream* newStream = OpenFile(Filename.c_str(), newAccess, UseURLs);
	if (!newStream) {
		return false;
	}

	StreamPtr->Close();
	StreamPtr = newStream;

	CurrentAccess = newAccess;

	return true;
}

bool FileStream::IsReadable() {
	return CurrentAccess == FileStream::READ_ACCESS;
}
bool FileStream::IsWritable() {
	return !IsReadable();
}
bool FileStream::MakeReadable(bool readable) {
	if (!readable) {
		return MakeWritable(true);
	}

	return Reopen(READ_ACCESS);
}
bool FileStream::MakeWritable(bool writable) {
	if (!writable) {
		return MakeReadable(true);
	}

	return Reopen(WRITE_ACCESS);
}

void FileStream::Close() {
	StreamPtr->Close();
	StreamPtr = nullptr;
	Stream::Close();
}
void FileStream::Seek(Sint64 offset) {
	StreamPtr->Seek(offset);
}
void FileStream::SeekEnd(Sint64 offset) {
	StreamPtr->SeekEnd(offset);
}
void FileStream::Skip(Sint64 offset) {
	StreamPtr->Skip(offset);
}
size_t FileStream::Position() {
	return StreamPtr->Position();
}
size_t FileStream::Length() {
	return StreamPtr->Length();
}

size_t FileStream::ReadBytes(void* data, size_t n) {
	return StreamPtr->ReadBytes(data, n);
}

size_t FileStream::WriteBytes(void* data, size_t n) {
	return StreamPtr->WriteBytes(data, n);
}
