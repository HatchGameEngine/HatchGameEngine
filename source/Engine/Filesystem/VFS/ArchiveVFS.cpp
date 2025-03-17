#include <Engine/Filesystem/VFS/ArchiveVFS.h>

bool ArchiveVFS::PutFile(const char* filename, VFSEntry* entry) {
	if (IsWritable()) {
		if (!HasFile(filename)) {
			NumEntries++;
		}

		Entries[filename] = entry;

		return true;
	}

	return false;
}

bool ArchiveVFS::EraseFile(const char* filename) {
	if (IsWritable()) {
		VFSEntryMap::iterator it = Entries.find(std::string(filename));

		if (it != Entries.end()) {
			Entries.erase(it);
			NumEntries--;

			return true;
		}
	}

	return false;
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

	VirtualFileSystem::Close();
}

ArchiveVFS::~ArchiveVFS() {
	Close();
}
