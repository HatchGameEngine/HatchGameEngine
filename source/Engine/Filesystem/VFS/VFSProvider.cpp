#include <Engine/Filesystem/VFS/VFSProvider.h>

VFSProvider::VFSProvider(Uint16 flags) {
	Flags = flags;
}
VFSProvider::~VFSProvider() {
	Close();
}

bool VFSProvider::IsOpen() {
	return Opened;
}

bool VFSProvider::IsReadable() {
	return (Flags & VFS_READABLE) != 0;
}

bool VFSProvider::IsWritable() {
	return (Flags & VFS_WRITABLE) != 0;
}

std::string VFSProvider::TransformFilename(const char* filename) {
	return std::string(filename);
}

bool VFSProvider::HasFile(const char* filename) {
	return false;
}
VFSEntry* VFSProvider::FindFile(const char* filename) {
	return nullptr;
}
bool VFSProvider::ReadFile(const char* filename, Uint8** out, size_t* size) {
	return false;
}

bool VFSProvider::PutFile(const char* filename, VFSEntry* entry) {
	return false;
}
bool VFSProvider::EraseFile(const char* filename) {
	return false;
}

void VFSProvider::AddOpenStream(VFSEntry* entry, Stream* stream) {
	VFSOpenStream openStream;
	openStream.EntryPtr = entry;
	openStream.StreamPtr = stream;
	OpenStreams.push_back(openStream);
}

Stream* VFSProvider::OpenReadStream(const char* filename) {
	return nullptr;
}
Stream* VFSProvider::OpenWriteStream(const char* filename) {
	return nullptr;
}
Stream* VFSProvider::OpenAppendStream(const char* filename) {
	return nullptr;
}
bool VFSProvider::CloseStream(Stream* stream) {
	for (size_t i = 0; i < OpenStreams.size(); i++) {
		VFSOpenStream& openStream = OpenStreams[i];

		if (openStream.StreamPtr == stream) {
			stream->Close();
			OpenStreams.erase(OpenStreams.begin() + i);
			return true;
		}
	}

	return false;
}

void VFSProvider::Close() {
	Opened = false;
}
