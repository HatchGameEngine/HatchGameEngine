#include <Engine/Filesystem/VFS/VFSProvider.h>

#include <Engine/Utilities/StringUtils.h>

VFSProvider::VFSProvider(const char *mountPoint, Uint16 flags) {
	MountPoint = StringUtils::LexicallyNormalFormOfPath(mountPoint);
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

const char* VFSProvider::GetMountPoint() {
	return MountPoint.c_str();
}

void VFSProvider::TransformFilename(const char* filename, char* dest, size_t destSize) {
	size_t offset = 0;

	if (StringUtils::StartsWith(filename, MountPoint.c_str())) {
		offset = MountPoint.size();
	}

	StringUtils::Copy(dest, filename + offset, destSize);
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

Stream* VFSProvider::OpenReadStream(const char* filename) {
	return nullptr;
}
Stream* VFSProvider::OpenWriteStream(const char* filename) {
	return nullptr;
}
bool VFSProvider::CloseStream(Stream* stream) {
	for (size_t i = 0; i < OpenStreams.size(); i++) {
		Stream* openStream = OpenStreams[i];

		if (openStream == stream) {
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
