#include <Engine/Filesystem/VFS/MemoryVFS.h>

bool MemoryVFS::Open() {
	Opened = true;

	return true;
}

bool MemoryVFS::ReadFile(const char* filename, Uint8** out, size_t* size) {
	VFSEntry* entry = FindFile(filename);
	if (entry == nullptr || entry->CachedData == nullptr) {
		return false;
	}

	Uint8* memory = (Uint8*)Memory::Malloc(entry->Size);
	if (!memory) {
		return false;
	}

	memcpy(memory, entry->CachedData, entry->Size);

	*out = memory;
	*size = (size_t)entry->Size;

	return true;
}

bool MemoryVFS::ReadEntryData(VFSEntry &entry, Uint8 *memory, size_t memSize){
	if (entry.CachedData) {
		size_t copyLength = entry.Size;
		if (copyLength > memSize) {
			copyLength = memSize;
		}

		memcpy(entry.CachedData, memory, copyLength);

		return true;
	}

	return false;
}

bool MemoryVFS::Flush() {
	return true;
}

MemoryVFS::~MemoryVFS() {
	Close();
}
