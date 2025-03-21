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
	StandardIOStream* stream = new (std::nothrow) StandardIOStream;
	if (!stream) {
		return nullptr;
	}

	stream->f = OpenFile(filename, access);

	if (!stream->f) {
		goto FREE;
	}

	fseek(stream->f, 0, SEEK_END);
	stream->size = ftell(stream->f);
	fseek(stream->f, 0, SEEK_SET);

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

	fclose(f);
	f = newFile;

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
	fclose(f);
	f = nullptr;
	Stream::Close();
}
void StandardIOStream::Seek(Sint64 offset) {
	fseek(f, offset, SEEK_SET);
}
void StandardIOStream::SeekEnd(Sint64 offset) {
	fseek(f, offset, SEEK_END);
}
void StandardIOStream::Skip(Sint64 offset) {
	fseek(f, offset, SEEK_CUR);
}
size_t StandardIOStream::Position() {
	return ftell(f);
}
size_t StandardIOStream::Length() {
	return size;
}

size_t StandardIOStream::ReadBytes(void* data, size_t n) {
	return fread(data, 1, n, f);
}

size_t StandardIOStream::WriteBytes(void* data, size_t n) {
	return fwrite(data, 1, n, f);
}
