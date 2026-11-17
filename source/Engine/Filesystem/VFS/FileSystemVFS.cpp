#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
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

bool FileSystemVFS::IsEmpty() {
	return Directory::IsEmpty(ParentPath.c_str());
}

bool FileSystemVFS::HasFile(const char* filename) {
	if (!IsReadable()) {
		return false;
	}

	Stream* stream = OpenStream(filename, FileStream::READ_ACCESS);
	if (!stream) {
		return false;
	}

	stream->Close();

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
		entry->Name = entryName;
		Cache[entryName] = entry;
	}

	entry->Size = stream != nullptr ? stream->Length() : 0;
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
	Stream* stream = OpenStream(filename, FileStream::READ_ACCESS);
	if (!stream) {
		return nullptr;
	}

	VFSEntry* entry = CacheEntry(filename, stream);

	stream->Close();

	return entry;
}

bool FileSystemVFS::ReadFile(const char* filename, Uint8** out, size_t* size) {
	if (!IsReadable()) {
		return false;
	}

	Stream* stream = OpenStream(filename, FileStream::READ_ACCESS);
	if (!stream) {
		return false;
	}

	VFSEntry* entry = CacheEntry(filename, stream);

	size_t length = entry->Size;

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

bool FileSystemVFS::CreateFile(const char* filename) {
	// See if the file exists already
	char resourcePath[MAX_RESOURCE_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return false;
	}

	// Cannot create if the path matches a directory. Return false.
	if (Directory::Exists(resourcePath)) {
		return false;
	}
	// If there is already a file of this name, return true.
	else if (File::Exists(resourcePath)) {
		return true;
	}

	// Else, the file doesn't exist. Create it.

	// Not writable
	if (!IsWritable()) {
		return false;
	}

	// First, recursively create the path directories if needed.
	if (!Path::Create(resourcePath)) {
		return false;
	}

	// Try opening the stream.
	// But don't write anything to it. Just check if it can be opened.
	Stream* stream = FileStream::New(resourcePath, FileStream::WRITE_ACCESS, true);
	if (stream == nullptr) {
		return false;
	}

	// Close it immediately
	stream->Close();

	// Cache the entry
	CacheEntry(filename, nullptr);

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

VFSEnumeration FileSystemVFS::EnumerateFiles(const char* path, VFSEnumerationOptions options) {
	VFSEnumeration enumeration;

	std::string fullPath = ParentPath;

	if (path != nullptr && path[0] != '\0') {
		std::filesystem::path pathToEnumerate = std::filesystem::u8path(std::string(path));
		pathToEnumerate = pathToEnumerate.lexically_normal();

		fullPath = Path::Concat(fullPath, Path::ToString(pathToEnumerate));
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

	Directory::SearchOptions searchOptions;
	searchOptions.AllDirs = !options.OnlyCurrentDirectory;
	searchOptions.ListDirs = !searchOptions.AllDirs;

	std::vector<std::filesystem::path> results;
	Directory::GetFiles(&results, fullPath.c_str(), "*", searchOptions);
	Directory::SortEntries(&results);

	for (size_t i = 0; i < results.size(); i++) {
		std::string filename = Path::ToString(results[i]);
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

Stream* FileSystemVFS::OpenStream(const char* filename, Uint32 access) {
	char resourcePath[MAX_RESOURCE_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return nullptr;
	}

	return FileStream::New(resourcePath, access, true);
}

Stream* FileSystemVFS::OpenReadStream(const char* filename) {
	Stream* stream = OpenStream(filename, FileStream::READ_ACCESS);
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
	Stream* stream = OpenStream(filename, FileStream::APPEND_ACCESS);
	if (stream != nullptr) {
		VFSEntry* entry = CacheEntry(filename, stream);
		AddOpenStream(entry, stream);
	}

	return stream;
}

void FileSystemVFS::Close() {
	VFSProvider::Close();

	for (VFSEntryMap::iterator it = Cache.begin(); it != Cache.end(); it++) {
		delete it->second;
	}

	Cache.clear();
}

FileSystemVFS::~FileSystemVFS() {
	Close();
}
