#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Filesystem/VFS/FileSystemVFS.h>
#include <Engine/IO/FileStream.h>
#include <Engine/Utilities/StringUtils.h>

bool FileSystemVFS::Open(const char* path) {
	ParentPath = std::string(path);
	Opened = true;

	return true;
}

bool FileSystemVFS::GetPath(const char* filename, char* path, size_t pathSize) {
	std::string fullPath = Path::Concat(ParentPath, std::string(filename));

	if (!StringUtils::StartsWith(fullPath, ParentPath)) {
		return false;
	}

	StringUtils::Copy(path, fullPath, pathSize);

	if (strchr(path, '\\') != nullptr) {
		// Nuh uh.
		return false;
	}

	if (Path::HasRelativeComponents(path)) {
		return false;
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
	std::string entryName = TransformFilename(filename);

	VFSEntry* entry = nullptr;
	VFSEntryMap::iterator it = Cache.find(entryName);
	if (it != Cache.end()) {
		entry = it->second;
	}

	if (entry == nullptr) {
		entry = new VFSEntry();
		Cache[entryName] = entry;
	}

	entry->Size = stream->Length();
	entry->CompressedSize = entry->Size;

	return entry;
}

VFSEntry* FileSystemVFS::GetCachedEntry(const char* filename) {
	std::string entryName = TransformFilename(filename);
	VFSEntryMap::iterator it = Cache.find(entryName);
	if (it != Cache.end()) {
		return it->second;
	}

	return nullptr;
}

void FileSystemVFS::RemoveFromCache(const char* filename) {
	std::string entryName = TransformFilename(filename);
	VFSEntryMap::iterator it = Cache.find(entryName);
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

	char resourcePath[MAX_RESOURCE_PATH_LENGTH];
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

VFSEnumeration FileSystemVFS::EnumerateFiles(const char* path) {
	VFSEnumeration enumeration;

	std::string fullPath = ParentPath;

	if (path != nullptr && path[0] != '\0') {
		std::filesystem::path pathToEnumerate = std::filesystem::u8path(std::string(path));
		pathToEnumerate = pathToEnumerate.lexically_normal();

		fullPath = Path::Concat(fullPath, pathToEnumerate.string());
	}

	size_t fullPathLength = fullPath.size();
	if (fullPathLength == 0 || Path::HasRelativeComponents(fullPath.c_str())) {
		enumeration.Result = VFSEnumerationResult::INVALID_PATH;
		return enumeration;
	}

	if (fullPath.back() != '/') {
		fullPath += "/";
		fullPathLength++;
	}

	std::vector<std::filesystem::path> results;
	Directory::GetFiles(&results, fullPath.c_str(), "*", true);

	for (size_t i = 0; i < results.size(); i++) {
		std::string filename = results[i].string();
		const char* relPath = filename.c_str() + fullPathLength;

		enumeration.Entries.push_back(std::string(relPath));
	}

	if (enumeration.Entries.size() > 0) {
		enumeration.Result = VFSEnumerationResult::SUCCESS;
	}
	else {
		enumeration.Result = VFSEnumerationResult::NO_RESULTS;
	}

	return enumeration;
}

Stream* FileSystemVFS::OpenReadStream(const char* filename) {
	char resourcePath[MAX_RESOURCE_PATH_LENGTH];
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
	char resourcePath[MAX_RESOURCE_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return nullptr;
	}

	// Recursively create the path directories if needed.
	if (!Path::Create(resourcePath)) {
		return nullptr;
	}

	Stream* stream = FileStream::New(resourcePath, FileStream::WRITE_ACCESS, true);
	if (stream != nullptr) {
		VFSEntry* entry = CacheEntry(filename, stream);
		AddOpenStream(entry, stream);
	}

	return stream;
}

Stream* FileSystemVFS::OpenAppendStream(const char* filename) {
	char resourcePath[MAX_RESOURCE_PATH_LENGTH];
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
	for (VFSEntryMap::iterator it = Cache.begin(); it != Cache.end(); it++) {
		delete it->second;
	}

	Cache.clear();

	VFSProvider::Close();
}

FileSystemVFS::~FileSystemVFS() {
	Close();
}
