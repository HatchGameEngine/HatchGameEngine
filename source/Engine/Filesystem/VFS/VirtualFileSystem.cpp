#include <Engine/Filesystem/VFS/VirtualFileSystem.h>

#include <Engine/Utilities/StringUtils.h>

VirtualFileSystem::VirtualFileSystem(Uint16 flags) {
	Flags = flags;
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

void VirtualFileSystem::TransformFilename(const char* filename, char* dest, size_t destSize) {
	StringUtils::Copy(dest, filename, destSize);
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

void VirtualFileSystem::Close() {
	Opened = false;
}

VirtualFileSystem::~VirtualFileSystem() {
	Close();
}
