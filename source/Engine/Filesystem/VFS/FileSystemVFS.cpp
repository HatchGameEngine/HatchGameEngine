#include <Engine/Filesystem/VFS/FileSystemVFS.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Utilities/StringUtils.h>

bool FileSystemVFS::Open(const char* path) {
	ParentPath = StringUtils::Duplicate(path);
	Opened = true;

	return true;
}

bool FileSystemVFS::HasFile(const char* filename) {
	if (!IsReadable()) {
		return false;
	}

	char resourcePath[4096];
	snprintf(resourcePath, sizeof resourcePath, "%s/%s", ParentPath, filename);

	SDL_RWops* rw = SDL_RWFromFile(resourcePath, "rb");
	if (!rw) {
		return false;
	}

	SDL_RWclose(rw);

	return true;
}

VFSEntry* FileSystemVFS::FindFile(const char* filename) {
	if (!IsReadable()) {
		return nullptr;
	}

	char resourcePath[4096];
	snprintf(resourcePath, sizeof resourcePath, "%s/%s", ParentPath, filename);

	SDL_RWops* rw = SDL_RWFromFile(resourcePath, "rb");
	if (!rw) {
		return nullptr;
	}

	Sint64 rwSize = SDL_RWsize(rw);
	if (rwSize < 0) {
		Log::Print(Log::LOG_ERROR,
			"Could not get size of file \"%s\": %s",
			resourcePath,
			SDL_GetError());
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

	entry->Size = rwSize;
	entry->CompressedSize = rwSize;

	SDL_RWclose(rw);

	return entry;
}

bool FileSystemVFS::ReadFile(const char* filename, Uint8** out, size_t* size) {
	if (!IsReadable()) {
		return false;
	}

	char resourcePath[4096];
	snprintf(resourcePath, sizeof resourcePath, "%s/%s", ParentPath, filename);

	SDL_RWops* rw = SDL_RWFromFile(resourcePath, "rb");
	if (!rw) {
		return false;
	}

	Sint64 rwSize = SDL_RWsize(rw);
	if (rwSize < 0) {
		Log::Print(Log::LOG_ERROR,
			"Could not get size of file \"%s\": %s",
			resourcePath,
			SDL_GetError());
		return false;
	}

	Uint8* memory = (Uint8*)Memory::Malloc(rwSize);
	if (!memory) {
		return false;
	}

	SDL_RWread(rw, memory, rwSize, 1);
	SDL_RWclose(rw);

	*out = memory;
	*size = rwSize;
	return true;
}

bool FileSystemVFS::PutFile(const char* filename, VFSEntry* entry) {
	if (!IsWritable()) {
		return false;
	}

	// TODO: Implement
	return false;
}

bool FileSystemVFS::EraseFile(const char* filename) {
	if (!IsWritable()) {
		return false;
	}

	// TODO: Implement
	return false;
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
