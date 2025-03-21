#include <Engine/Filesystem/VFS/VFSProvider.h>

VFSProvider::VFSProvider(Uint16 flags) {
	Flags = flags;
}
VFSProvider::~VFSProvider() {
	if (Opened) {
		Close();
	}
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
bool VFSProvider::CanUnmount() {
	return OpenStreams.size() == 0;
}

void VFSProvider::SetWritable(bool writable) {
	if (writable) {
		Flags |= VFS_WRITABLE;
	}
	else {
		Flags &= ~VFS_WRITABLE;
	}
}

std::string VFSProvider::TransformFilename(const char* filename) {
	return std::string(filename);
}

bool VFSProvider::SupportsCompression() {
	return false;
}
bool VFSProvider::SupportsEncryption() {
	return false;
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

// Note that this may return the transformed filenames, depending on the provider.
VFSEnumeration VFSProvider::EnumerateFiles(const char* path) {
	VFSEnumeration enumeration;
	enumeration.Result = VFSEnumerationResult::NO_RESULTS;

	return enumeration;
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
	for (size_t i = 0; i < OpenStreams.size(); i++) {
		VFSOpenStream& openStream = OpenStreams[i];
		openStream.StreamPtr->Close();
	}

	Opened = false;
}
