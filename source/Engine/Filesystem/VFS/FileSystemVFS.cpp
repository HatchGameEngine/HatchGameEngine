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

VFSEntry* FileSystemVFS::FindFile(const char* filename) {
	if (!IsReadable()) {
		return nullptr;
	}

	Stream* stream = OpenReadStream(filename);
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

	CloseStream(stream);

	return entry;
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

	return (success == 0);
}

Stream* FileSystemVFS::OpenReadStream(const char* filename) {
	char resourcePath[MAX_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return nullptr;
	}

	return FileStream::New(resourcePath, FileStream::READ_ACCESS, true);
}

Stream* FileSystemVFS::OpenWriteStream(const char* filename) {
	char resourcePath[MAX_PATH_LENGTH];
	if (!GetPath(filename, resourcePath, sizeof resourcePath)) {
		return nullptr;
	}

	return FileStream::New(resourcePath, FileStream::WRITE_ACCESS, true);
}

bool FileSystemVFS::CloseStream(Stream* stream) {
	if (stream) {
		stream->Close();

		return true;
	}

	return false;
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
