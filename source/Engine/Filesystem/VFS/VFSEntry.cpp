#include <Engine/Filesystem/VFS/VFSEntry.h>

#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/MemoryStream.h>

VFSEntry* VFSEntry::FromStream(const char* filename, Stream* stream) {
	size_t length = stream->Length();
	MemoryStream* memStream = MemoryStream::New(length);
	if (memStream == nullptr) {
		return nullptr;
	}

	VFSEntry* entry = new VFSEntry();
	entry->Name = std::string(filename);
	entry->Offset = 0;
	entry->Size = length;
	entry->Flags = entry->FileFlags = 0;
	entry->CompressedSize = entry->Size;

	stream->CopyTo(memStream);
	memStream->owns_memory = false;
	memStream->Close();

	entry->CachedData = memStream->pointer_start;

	return entry;
}
