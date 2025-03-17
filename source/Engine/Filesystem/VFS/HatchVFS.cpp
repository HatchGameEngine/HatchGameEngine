#include <Engine/Filesystem/VFS/HatchVFS.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/IO/Compression/ZLibStream.h>

#define ENTRY_NAME_LENGTH (8 * 2)

bool HatchVFS::Open(Stream* stream) {
	Uint16 fileCount;
	Uint8 magicHATCH[5];
	stream->ReadBytes(magicHATCH, 5);
	if (memcmp(magicHATCH, "HATCH", 5)) {
		Log::Print(Log::LOG_ERROR,
			"Invalid HATCH data file! (%02X %02X %02X %02X %02X)",
			magicHATCH[0],
			magicHATCH[1],
			magicHATCH[2],
			magicHATCH[3],
			magicHATCH[4]);
		return false;
	}

	// Uint8 major, minor, pad;
	stream->ReadByte();
	stream->ReadByte();
	stream->ReadByte();

	fileCount = stream->ReadUInt16();
	Log::Print(Log::LOG_VERBOSE, "Loading resource table from HATCH data file...");
	for (int i = 0; i < fileCount; i++) {
		Uint32 crc32 = stream->ReadUInt32();
		Uint64 offset = stream->ReadUInt64();
		Uint64 size = stream->ReadUInt64();
		Uint32 dataFlag = stream->ReadUInt32();
		Uint64 compressedSize = stream->ReadUInt64();

		Uint32 entryFlags = 0;
		if (dataFlag == 2) {
			entryFlags |= VFSE_ENCRYPTED;
		}

		char entryName[ENTRY_NAME_LENGTH + 1];
		snprintf(entryName, ENTRY_NAME_LENGTH + 1, "%08x", crc32);

		VFSEntry* entry = new VFSEntry();
		entry->Name = std::string(entryName);
		entry->Offset = offset;
		entry->Size = size;
		entry->Flags = entryFlags;
		entry->CompressedSize = compressedSize;

		Entries[entryName] = entry;
		NumEntries++;
	}

	StreamPtr = stream;
	Opened = true;

	return true;
}

void HatchVFS::TransformFilename(const char* filename, char* dest, size_t destSize) {
	Uint32 filenameHash = CRC32::EncryptString(filename);
	snprintf(dest, destSize, "%08x", filenameHash);
}

bool HatchVFS::ReadFile(const char* filename, Uint8** out, size_t* size) {
	VFSEntry* entry = FindFile(filename);
	if (entry == nullptr) {
		return false;
	}

	Uint8* memory = (Uint8*)Memory::Malloc(entry->Size);
	if (!memory) {
		return false;
	}

	StreamPtr->Seek(entry->Offset);
	if (entry->Size != entry->CompressedSize) {
		Uint8* compressedMemory = (Uint8*)Memory::Malloc(entry->Size);
		if (!compressedMemory) {
			Memory::Free(memory);
			return false;
		}
		StreamPtr->ReadBytes(compressedMemory, entry->CompressedSize);

		ZLibStream::Decompress(
			memory, (size_t)entry->Size, compressedMemory, (size_t)entry->CompressedSize);
		Memory::Free(compressedMemory);
	}
	else {
		StreamPtr->ReadBytes(memory, entry->Size);
	}

	if (entry->Flags & VFSE_ENCRYPTED) {
		Uint8 keyA[16];
		Uint8 keyB[16];
		Uint32 filenameHash = (int)strtol(filename, NULL, 16);
		Uint64 leSize = TO_LE64(entry->Size);
		Uint32 sizeHash = CRC32::EncryptData(&leSize, sizeof(leSize));

		// Populate Key A
		Uint32* keyA32 = (Uint32*)&keyA[0];
		keyA32[0] = TO_LE32(filenameHash);
		keyA32[1] = TO_LE32(filenameHash);
		keyA32[2] = TO_LE32(filenameHash);
		keyA32[3] = TO_LE32(filenameHash);

		// Populate Key B
		Uint32* keyB32 = (Uint32*)&keyB[0];
		keyB32[0] = TO_LE32(sizeHash);
		keyB32[1] = TO_LE32(sizeHash);
		keyB32[2] = TO_LE32(sizeHash);
		keyB32[3] = TO_LE32(sizeHash);

		int swapNibbles = 0;
		int indexKeyA = 0;
		int indexKeyB = 8;
		int xorValue = (entry->Size >> 2) & 0x7F;
		for (Uint32 x = 0; x < entry->Size; x++) {
			Uint8 temp = memory[x];

			temp ^= xorValue ^ keyB[indexKeyB++];

			if (swapNibbles) {
				temp = (((temp & 0x0F) << 4) | ((temp & 0xF0) >> 4));
			}

			temp ^= keyA[indexKeyA++];

			memory[x] = temp;

			if (indexKeyA <= 15) {
				if (indexKeyB > 12) {
					indexKeyB = 0;
					swapNibbles ^= 1;
				}
			}
			else if (indexKeyB <= 8) {
				indexKeyA = 0;
				swapNibbles ^= 1;
			}
			else {
				xorValue = (xorValue + 2) & 0x7F;
				if (swapNibbles) {
					swapNibbles = false;
					indexKeyA = xorValue % 7;
					indexKeyB = (xorValue % 12) + 2;
				}
				else {
					swapNibbles = true;
					indexKeyA = (xorValue % 12) + 3;
					indexKeyB = xorValue % 7;
				}
			}
		}
	}

	*out = memory;
	*size = (size_t)entry->Size;
	return true;
}

bool HatchVFS::PutFile(const char* filename, VFSEntry* entry) {
	// Cannot write to a .hatch file
	return false;
}

bool HatchVFS::EraseFile(const char* filename) {
	// Cannot write to a .hatch file
	return false;
}

void HatchVFS::Close() {
	if (StreamPtr) {
		StreamPtr->Close();
		StreamPtr = nullptr;
	}

	ArchiveVFS::Close();
}

HatchVFS::~HatchVFS() {
	Close();
}
