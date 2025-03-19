#include <Engine/Filesystem/VFS/FileSystemVFS.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/IO/FileStream.h>
#include <Engine/Utilities/StringUtils.h>

bool FileSystemVFS::Open(const char* path) {
	ParentPath = std::string(path);
	Opened = true;

	return true;
}

bool FileSystemVFS::GetPath(const char* filename, char* path, size_t pathSize) {
	// TODO: Validate the path here.
	const char* parentPath = ParentPath.c_str();
	if (parentPath[ParentPath.size() - 1] == '/') {
		snprintf(path, pathSize, "%s%s", parentPath, filename);
	}
	else {
		snprintf(path, pathSize, "%s/%s", parentPath, filename);
	}

	return true;
}

bool FileSystemVFS::HasFile(const char* filename) {
	if (!IsReadable()) {
		return false;
	}

	Stream* stream = OpenReadStream(filename);
	if (!stream) {
		return false;
	}

	CloseStream(stream);

	return true;
}

VFSEntry* FileSystemVFS::CacheEntry(const char* filename, Stream* stream) {
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

	return entry;
}

VFSEntry* FileSystemVFS::GetCachedEntry(const char* filename) {
	VFSEntryMap::iterator it = Cache.find(std::string(filename));
	if (it != Cache.end()) {
		return it->second;
	}

	return nullptr;
}

void FileSystemVFS::RemoveFromCache(const char* filename) {
	VFSEntryMap::iterator it = Cache.find(std::string(filename));
	if (it != Cache.end()) {
		delete it->second;
	}
}

VFSEntry* FileSystemVFS::FindFile(const char* filename) {
	if (!IsReadable()) {
		return nullptr;
	}

	// This caches the file entry, but not its data.
	Stream* stream = OpenReadStream(filename);
	if (!stream) {
		return nullptr;
	}

	CloseStream(stream);

	return GetCachedEntry(filename);
}

bool FileSystemVFS::ReadFile(const char* filename, Uint8** out, size_t* size) {
	if (!IsReadable()) {
		return false;
	}

	Stream* stream = OpenReadStream(filename);
	if (!stream) {
		return false;
	}

	size_t length = stream->Length();

	Uint8* memory = (Uint8*)Memory::Malloc(length);
	if (!memory) {
		return false;
	}

	stream->ReadBytes(memory, length);

	CloseStream(stream);

	*out = memory;
	*size = length;

	return true;
}

bool FileSystemVFS::PutFile(const char* filename, VFSEntry* entry) {
	if (!IsWritable()) {
		return false;
	}

	Stream* stream = OpenWriteStream(filename);
	if (!stream) {
		return false;
	}

	stream->WriteBytes(entry->CachedData, entry->Size);

	CacheEntry(filename, stream);

	CloseStream(stream);

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
	if (success == 0) {
		RemoveFromCache(filename);

		return true;
	}

	return false;
}

Stream* FileSystemVFS::OpenReadStream(const char* filename) {
	char resourcePath[MAX_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return nullptr;
	}

	Stream* stream = FileStream::New(resourcePath, FileStream::READ_ACCESS, true);
	if (stream != nullptr) {
		VFSEntry* entry = CacheEntry(filename, stream);
		AddOpenStream(entry, stream);
	}

	return stream;
}

Stream* FileSystemVFS::OpenWriteStream(const char* filename) {
	char resourcePath[MAX_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return nullptr;
	}

	// Recursively create the path directories if needed.
	Path::Create(resourcePath);

	Stream* stream = FileStream::New(resourcePath, FileStream::WRITE_ACCESS, true);
	if (stream != nullptr) {
		VFSEntry* entry = CacheEntry(filename, stream);
		AddOpenStream(entry, stream);
	}

	return stream;
}

Stream* FileSystemVFS::OpenAppendStream(const char* filename) {
	char resourcePath[MAX_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return nullptr;
	}

	Stream* stream = FileStream::New(resourcePath, FileStream::APPEND_ACCESS, true);
	if (stream != nullptr) {
		VFSEntry* entry = CacheEntry(filename, stream);
		AddOpenStream(entry, stream);
	}

	return stream;
}

void FileSystemVFS::Close() {
	for (VFSEntryMap::iterator it = Cache.begin();
		it != Cache.end();
		it++) {
		delete it->second;
	}

	VFSProvider::Close();
}

FileSystemVFS::~FileSystemVFS() {
	Close();
}
