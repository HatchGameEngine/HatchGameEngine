#include <Engine/Filesystem/VFS/ArchiveVFS.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/Utilities/StringUtils.h>

bool ArchiveVFS::AddEntry(VFSEntry* entry) {
	// This adds the entry directly, without doing any filename transformation
	// or modifying the entry name.
	// If a file with the same name already exists, it's not added.
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
		std::string entryName = TransformFilename(filename);

		VFSEntryMap::iterator it = Entries.find(entryName);

		return it != Entries.end();
	}

	return false;
}

VFSEntry* ArchiveVFS::FindFile(const char* filename) {
	if (IsReadable()) {
		std::string entryName = TransformFilename(filename);

		VFSEntryMap::iterator it = Entries.find(entryName);
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
	else if (!ReadEntryData(*entry, memory, entry->Size)) {
		Memory::Free(memory);

		return false;
	}

	*out = memory;
	*size = (size_t)entry->Size;

	return true;
}

bool ArchiveVFS::ReadEntryData(VFSEntry &entry, Uint8 *memory, size_t memSize) {
	return false;
}

bool ArchiveVFS::PutFile(const char* filename, VFSEntry* entry) {
	if (IsWritable()) {
		std::string entryName = TransformFilename(filename);

		VFSEntryMap::iterator it = Entries.find(entryName);
		if (it == Entries.end()) {
			EntryNames.push_back(entryName);
			NumEntries++;
		}

		entry->Name = entryName;

		Entries[entryName] = entry;

		return true;
	}

	return false;
}

bool ArchiveVFS::EraseFile(const char* filename) {
	if (IsWritable()) {
		std::string entryName = TransformFilename(filename);

		VFSEntryMap::iterator it = Entries.find(entryName);

		if (it != Entries.end()) {
			auto nameIt = std::find(EntryNames.begin(), EntryNames.end(), entryName);
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

VFSEnumeration ArchiveVFS::EnumerateFiles(const char* path) {
	VFSEnumeration enumeration;

	if (NumEntries == 0) {
		enumeration.Result = VFSEnumerationResult::NO_RESULTS;
		return enumeration;
	}

	for (size_t i = 0; i < NumEntries; i++) {
		std::string entryName = EntryNames[i];
		if (path != nullptr && path[0] != '\0' &&
			!StringUtils::StartsWith(entryName.c_str(), path)) {
			continue;
		}

		enumeration.Entries.push_back(entryName);
	}

	enumeration.Result = VFSEnumerationResult::SUCCESS;

	return enumeration;
}

Stream* ArchiveVFS::OpenMemStreamForEntry(VFSEntry &entry) {
	if (entry.CachedData == nullptr) {
		return nullptr;
	}

	MemoryStream* memStream = MemoryStream::New(entry.Size);
	if (memStream == nullptr) {
		return nullptr;
	}

	memset(memStream->pointer_start, 0x00, entry.Size);
	memcpy(memStream->pointer_start, entry.CachedData, entry.Size);

	return (Stream*)memStream;
}

Stream* ArchiveVFS::OpenReadStream(const char* filename) {
	VFSEntry* entry = FindFile(filename);
	if (entry == nullptr){
		return nullptr;
	}

	Stream* memStream = OpenMemStreamForEntry(*entry);
	if (memStream) {
		memStream->MakeWritable(false);
		AddOpenStream(entry, memStream);
	}

	return memStream;
}

Stream* ArchiveVFS::OpenWriteStream(const char* filename) {
	VFSEntry* entry = FindFile(filename);

	// If the entry does not exist, create one
	if (entry == nullptr) {
		entry = new VFSEntry();
		entry->CachedData = (Uint8*)Memory::Calloc(1, sizeof(Uint8));
		PutFile(filename, entry);
	}

	Stream* memStream = OpenMemStreamForEntry(*entry);
	if (memStream) {
		AddOpenStream(entry, memStream);
	}

	return memStream;
}

Stream* ArchiveVFS::OpenAppendStream(const char* filename) {
	VFSEntry* entry = FindFile(filename);
	if (entry == nullptr){
		return nullptr;
	}

	Stream* memStream = OpenMemStreamForEntry(*entry);
	if (memStream) {
		memStream->Seek(memStream->Length());
		AddOpenStream(entry, memStream);
	}

	return memStream;
}

bool ArchiveVFS::CloseStream(Stream* stream) {
	for (size_t i = 0; i < OpenStreams.size(); i++) {
		VFSOpenStream& openStream = OpenStreams[i];

		if (openStream.StreamPtr == stream) {
			// Flush to archive
			if (stream->IsWritable()) {
				VFSEntry* entry = openStream.EntryPtr;

				size_t newSize = stream->Length();
				Uint8* newMemory = nullptr;
				if (newSize != 0) {
					newMemory = (Uint8*)Memory::Calloc(newSize, sizeof(Uint8));
				}
				else {
					Memory::Free(entry->CachedData);

					entry->CachedData = nullptr;
					entry->Size = 0;

					NeedsRepacking = true;
				}

				if (newMemory != nullptr) {
					stream->Seek(0);
					stream->ReadBytes(newMemory, newSize);

					Memory::Free(entry->CachedData);

					entry->CachedData = newMemory;
					entry->Size = newSize;

					NeedsRepacking = true;
				}
			}

			stream->Close();

			OpenStreams.erase(OpenStreams.begin() + i);
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
		for (VFSEntryMap::iterator it = Entries.begin(); it != Entries.end(); it++) {
			delete it->second;
		}

		NumEntries = 0;
	}

	Entries.clear();
	EntryNames.clear();

	VFSProvider::Close();
}

ArchiveVFS::~ArchiveVFS() {
	Close();
}
