#include <Engine/Filesystem/Directory.h>
#include <Engine/IO/StandardIOStream.h>

FILE* StandardIOStream::OpenFile(const char* filename, Uint32 access) {
	const char* accessString = nullptr;

	switch (access) {
	case StandardIOStream::READ_ACCESS:
		accessString = "rb";
		break;
	case StandardIOStream::WRITE_ACCESS:
		accessString = "wb";
		break;
	case StandardIOStream::APPEND_ACCESS:
		accessString = "ab";
		break;
	}

	return fopen(filename, accessString);
}

StandardIOStream* StandardIOStream::New(const char* filename, Uint32 access) {
	// Cannot open file stream if there is a directory with that name.
	if (Directory::Exists(filename)) {
		return nullptr;
	}

	FILE* file = OpenFile(filename, access);
	if (!file) {
		return nullptr;
	}

	StandardIOStream* stream = new (std::nothrow) StandardIOStream;
	if (!stream) {
		return nullptr;
	}

	stream->FilePtr = file;

	fseek(file, 0, SEEK_END);
	stream->Filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	stream->Filename = std::string(filename);
	stream->CurrentAccess = access;

	return stream;

FREE:
	delete stream;
	return nullptr;
}

bool StandardIOStream::Reopen(Uint32 newAccess) {
	if (CurrentAccess == newAccess) {
		return true;
	}

	FILE* newFile = OpenFile(Filename.c_str(), newAccess);
	if (!newFile) {
		return false;
	}

	if (FilePtr) {
		fclose(FilePtr);
	}
	FilePtr = newFile;

	CurrentAccess = newAccess;

	return true;
}

bool StandardIOStream::IsReadable() {
	return CurrentAccess == StandardIOStream::READ_ACCESS;
}
bool StandardIOStream::IsWritable() {
	return !IsReadable();
}
bool StandardIOStream::MakeReadable(bool readable) {
	if (!readable) {
		return MakeWritable(true);
	}

	return Reopen(READ_ACCESS);
}
bool StandardIOStream::MakeWritable(bool writable) {
	if (!writable) {
		return MakeReadable(true);
	}

	return Reopen(WRITE_ACCESS);
}

void StandardIOStream::Close() {
	fclose(FilePtr);
	FilePtr = nullptr;
	Stream::Close();
}
void StandardIOStream::Seek(Sint64 offset) {
	fseek(FilePtr, offset, SEEK_SET);
}
void StandardIOStream::SeekEnd(Sint64 offset) {
	fseek(FilePtr, offset, SEEK_END);
}
void StandardIOStream::Skip(Sint64 offset) {
	fseek(FilePtr, offset, SEEK_CUR);
}
size_t StandardIOStream::Position() {
	return ftell(FilePtr);
}
size_t StandardIOStream::Length() {
	return Filesize;
}

size_t StandardIOStream::ReadBytes(void* data, size_t n) {
	return fread(data, 1, n, FilePtr);
}

size_t StandardIOStream::WriteBytes(void* data, size_t n) {
	return fwrite(data, 1, n, FilePtr);
}
