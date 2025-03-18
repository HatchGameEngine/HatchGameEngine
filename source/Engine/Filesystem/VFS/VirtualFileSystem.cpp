#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

#include <Engine/Utilities/StringUtils.h>

VirtualFileSystem::VirtualFileSystem(const char *mountPoint, Uint16 flags) {
	MountPoint = StringUtils::LexicallyNormalFormOfPath(mountPoint);
	Flags = flags;
}
VirtualFileSystem::~VirtualFileSystem() {
	Close();
}

bool VirtualFileSystem::IsOpen() {
	return Opened;
}

bool VirtualFileSystem::IsReadable() {
	return (Flags & VFS_READABLE) != 0;
}

bool VirtualFileSystem::IsWritable() {
	return (Flags & VFS_WRITABLE) != 0;
}

const char* VirtualFileSystem::GetMountPoint() {
	return MountPoint.c_str();
}

void VirtualFileSystem::TransformFilename(const char* filename, char* dest, size_t destSize) {
	size_t offset = 0;

	if (StringUtils::StartsWith(filename, MountPoint.c_str())) {
		offset = MountPoint.size();
	}

	StringUtils::Copy(dest, filename + offset, destSize);
}

bool VirtualFileSystem::HasFile(const char* filename) {
	return false;
}
VFSEntry* VirtualFileSystem::FindFile(const char* filename) {
	return nullptr;
}
bool VirtualFileSystem::ReadFile(const char* filename, Uint8** out, size_t* size) {
	return false;
}

bool VirtualFileSystem::PutFile(const char* filename, VFSEntry* entry) {
	return false;
}
bool VirtualFileSystem::EraseFile(const char* filename) {
	return false;
}

Stream* VirtualFileSystem::OpenReadStream(const char* filename) {
	return nullptr;
}
Stream* VirtualFileSystem::OpenWriteStream(const char* filename) {
	return nullptr;
}
bool VirtualFileSystem::CloseStream(Stream* stream) {
	return false;
}

void VirtualFileSystem::Close() {
	Opened = false;
}
