#include <Engine/IO/VirtualFileStream.h>

Stream* VirtualFileStream::OpenFile(VirtualFileSystem* vfs, const char* filename, Uint32 access) {
	switch (access) {
	case VirtualFileStream::READ_ACCESS:
		return vfs->OpenReadStream(filename);
	case VirtualFileStream::WRITE_ACCESS:
		return vfs->OpenWriteStream(filename);
	case VirtualFileStream::APPEND_ACCESS:
		return vfs->OpenAppendStream(filename);
	}

	return nullptr;
}

VirtualFileStream*
VirtualFileStream::New(VirtualFileSystem* vfs, const char* filename, Uint32 access) {
	VirtualFileStream* stream = new (std::nothrow) VirtualFileStream;
	if (!stream) {
		return nullptr;
	}

	stream->StreamPtr = OpenFile(vfs, filename, access);
	if (!stream->StreamPtr) {
		goto FREE;
	}

	stream->VFSPtr = vfs;
	stream->Filename = std::string(filename);
	stream->CurrentAccess = access;

	return stream;

FREE:
	delete stream;
	return nullptr;
}

bool VirtualFileStream::Reopen(Uint32 newAccess) {
	if (CurrentAccess == newAccess) {
		return true;
	}

	Stream* newStream = OpenFile(VFSPtr, Filename.c_str(), newAccess);
	if (!newStream) {
		return false;
	}

	VFSPtr->CloseStream(StreamPtr);

	StreamPtr = newStream;
	CurrentAccess = newAccess;

	return true;
}

bool VirtualFileStream::IsReadable() {
	return CurrentAccess == VirtualFileStream::READ_ACCESS;
}
bool VirtualFileStream::IsWritable() {
	return CurrentAccess == VirtualFileStream::WRITE_ACCESS;
}
bool VirtualFileStream::MakeReadable(bool readable) {
	if (!readable) {
		return MakeWritable(true);
	}

	return Reopen(READ_ACCESS);
}
bool VirtualFileStream::MakeWritable(bool writable) {
	if (!writable) {
		return MakeReadable(true);
	}

	return Reopen(WRITE_ACCESS);
}

void VirtualFileStream::Close() {
	VFSPtr->CloseStream(StreamPtr);

	VFSPtr = nullptr;
	StreamPtr = nullptr;

	Stream::Close();
}
void VirtualFileStream::Seek(Sint64 offset) {
	StreamPtr->Seek(offset);
}
void VirtualFileStream::SeekEnd(Sint64 offset) {
	StreamPtr->SeekEnd(offset);
}
void VirtualFileStream::Skip(Sint64 offset) {
	StreamPtr->Skip(offset);
}
size_t VirtualFileStream::Position() {
	return StreamPtr->Position();
}
size_t VirtualFileStream::Length() {
	return StreamPtr->Length();
}

size_t VirtualFileStream::ReadBytes(void* data, size_t n) {
	return StreamPtr->ReadBytes(data, n);
}

size_t VirtualFileStream::WriteBytes(void* data, size_t n) {
	return StreamPtr->WriteBytes(data, n);
}
