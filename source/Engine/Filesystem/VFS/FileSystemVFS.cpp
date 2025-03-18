#include <Engine/Filesystem/VFS/FileSystemVFS.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/IO/FileStream.h>
#include <Engine/Utilities/StringUtils.h>

bool FileSystemVFS::Open(const char* path) {
	ParentPath = StringUtils::Duplicate(path);
	Opened = true;

	return true;
}

bool FileSystemVFS::GetPath(const char* filename, char* path, size_t pathSize) {
	// TODO: Validate the path here.
	snprintf(path, pathSize, "%s/%s", ParentPath, filename);

	return true;
}

bool FileSystemVFS::HasFile(const char* filename) {
	if (!IsReadable()) {
		return false;
	}

	char resourcePath[MAX_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return false;
	}

	FileStream* stream = FileStream::New(resourcePath, FileStream::READ_ACCESS);
	if (!stream) {
		return false;
	}

	stream->Close();

	return true;
}

VFSEntry* FileSystemVFS::FindFile(const char* filename) {
	if (!IsReadable()) {
		return nullptr;
	}

	char resourcePath[MAX_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return nullptr;
	}

	FileStream* stream = FileStream::New(resourcePath, FileStream::READ_ACCESS);
	if (!stream) {
		return nullptr;
	}

	VFSEntry* entry = nullptr;
	VFSEntryMap::iterator it = Cache.find(std::string(filename));
	if (it != Cache.end()) {
		entry = it->second;
	}

	if (entry == nullptr) {
		entry = new VFSEntry();
		entry->Name = std::string(filename);
		Cache[entry->Name] = entry;
	}

	entry->Size = stream->Length();
	entry->CompressedSize = entry->Size;

	stream->Close();

	return entry;
}

bool FileSystemVFS::ReadFile(const char* filename, Uint8** out, size_t* size) {
	if (!IsReadable()) {
		return false;
	}

	char resourcePath[MAX_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return false;
	}

	FileStream* stream = FileStream::New(resourcePath, FileStream::READ_ACCESS);
	if (!stream) {
		return false;
	}

	size_t length = stream->Length();

	Uint8* memory = (Uint8*)Memory::Malloc(length);
	if (!memory) {
		return false;
	}

	stream->ReadBytes(memory, length);
	stream->Close();

	*out = memory;
	*size = length;

	return true;
}

bool FileSystemVFS::PutFile(const char* filename, VFSEntry* entry) {
	if (!IsWritable()) {
		return false;
	}

	char resourcePath[MAX_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return false;
	}

	FileStream* stream = FileStream::New(resourcePath, FileStream::WRITE_ACCESS);
	if (!stream) {
		return false;
	}

	stream->WriteBytes(entry->CachedData, entry->Size);
	stream->Close();

	return true;
}

bool FileSystemVFS::EraseFile(const char* filename) {
	if (!IsWritable()) {
		return false;
	}

	char resourcePath[MAX_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return false;
	}

	int success = remove(resourcePath);

	return (success == 0);
}

void FileSystemVFS::Close() {
	if (ParentPath) {
		Memory::Free(ParentPath);
		ParentPath = nullptr;
	}

	for (VFSEntryMap::iterator it = Cache.begin();
		it != Cache.end();
		it++) {
		delete it->second;
	}

	VirtualFileSystem::Close();
}

FileSystemVFS::~FileSystemVFS() {
	Close();
}
