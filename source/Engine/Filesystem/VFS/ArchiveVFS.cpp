#include <Engine/Filesystem/VFS/ArchiveVFS.h>

bool ArchiveVFS::AddEntry(VFSEntry* entry) {
	VFSEntryMap::iterator it = Entries.find(entry->Name);
	if (it != Entries.end()) {
		return false;
	}

	Entries[entry->Name] = entry;
	EntryNames.push_back(entry->Name);
	NumEntries++;

	return true;
}

bool ArchiveVFS::HasFile(const char* filename) {
	if (IsReadable()) {
		VFSEntryMap::iterator it = Entries.find(std::string(filename));

		return it != Entries.end();
	}

	return false;
}

VFSEntry* ArchiveVFS::FindFile(const char* filename) {
	if (IsReadable()) {
		VFSEntryMap::iterator it = Entries.find(std::string(filename));
		if (it != Entries.end()) {
			return it->second;
		}
	}

	return nullptr;
}

bool ArchiveVFS::ReadFile(const char* filename, Uint8** out, size_t* size) {
	VFSEntry* entry = FindFile(filename);
	if (entry == nullptr) {
		return false;
	}

	Uint8* memory = (Uint8*)Memory::Malloc(entry->Size);
	if (!memory) {
		return false;
	}

	// Read cached data if available
	if (entry->CachedData) {
		memcpy(memory, entry->CachedData, entry->Size);
	}
	// Else, read from file
	else if (!ReadEntryData(entry, memory)) {
		Memory::Free(memory);

		return false;
	}

	*out = memory;
	*size = (size_t)entry->Size;

	return true;
}

bool ArchiveVFS::ReadEntryData(VFSEntry* entry, Uint8* memory) {
	return false;
}

bool ArchiveVFS::PutFile(const char* filename, VFSEntry* entry) {
	if (IsWritable()) {
		std::string fName = std::string(filename);

		if (!HasFile(filename)) {
			EntryNames.push_back(fName);
			NumEntries++;
		}

		Entries[fName] = entry;

		return true;
	}

	return false;
}

bool ArchiveVFS::EraseFile(const char* filename) {
	if (IsWritable()) {
		std::string fName = std::string(filename);

		VFSEntryMap::iterator it = Entries.find(fName);

		if (it != Entries.end()) {
			auto nameIt = std::find(EntryNames.begin(), EntryNames.end(), fName);
			if (nameIt != EntryNames.end()) {
				EntryNames.erase(nameIt);
			}

			Entries.erase(it);
			NumEntries--;

			return true;
		}
	}

	return false;
}

bool ArchiveVFS::Flush() {
	return false;
}

void ArchiveVFS::Close() {
	if (NumEntries != 0) {
		for (VFSEntryMap::iterator it = Entries.begin();
			it != Entries.end();
			it++) {
			delete it->second;
		}

		NumEntries = 0;
	}

	EntryNames.clear();

	VirtualFileSystem::Close();
}

ArchiveVFS::~ArchiveVFS() {
	Close();
}
