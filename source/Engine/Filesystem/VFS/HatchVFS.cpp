#include <Engine/Filesystem/VFS/HatchVFS.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/Compression/ZLibStream.h>

#define MAGIC_HATCH_SIZE 5

char MAGIC_HATCH[MAGIC_HATCH_SIZE] = {0x48, 0x41, 0x54, 0x43, 0x48}; // HATCH

#define ENTRY_NAME_LENGTH 8

bool HatchVFS::Open(Stream* stream) {
	Uint16 fileCount;
	Uint8 magicHATCH[MAGIC_HATCH_SIZE];
	stream->ReadBytes(magicHATCH, MAGIC_HATCH_SIZE);
	if (memcmp(magicHATCH, MAGIC_HATCH, MAGIC_HATCH_SIZE)) {
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
	for (int i = 0; i < fileCount; i++) {
		Uint32 crc32 = stream->ReadUInt32();
		Uint64 offset = stream->ReadUInt64();
		Uint64 size = stream->ReadUInt64();
		Uint32 dataFlag = stream->ReadUInt32();
		Uint64 compressedSize = stream->ReadUInt64();

		Uint32 entryFlags = 0;
		if (size != compressedSize) {
			entryFlags |= VFSE_COMPRESSED;
		}
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

		if (!ArchiveVFS::AddEntry(entry)) {
			Log::Print(Log::LOG_WARN, "HATCH file has duplicate entry \"%s\", ignoring", entryName);
		}
	}

	StreamPtr = stream;
	Opened = true;

	return true;
}

void HatchVFS::TransformFilename(const char* filename, char* dest, size_t destSize) {
	VFSProvider::TransformFilename(filename, dest, destSize);

	snprintf(dest, destSize, "%08x", CRC32::EncryptString(dest));
}

void HatchVFS::CryptoXOR(Uint8* data, size_t size, Uint32 filenameHash, bool decrypt) {
	Uint8 keyA[16];
	Uint8 keyB[16];
	Uint64 leSize = TO_LE64(size);
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
	int xorValue = (size >> 2) & 0x7F;

	for (Uint32 x = 0; x < size; x++) {
		Uint8 temp = data[x];

		if (decrypt)
			temp ^= xorValue ^ keyB[indexKeyB++];
		else
			temp ^= keyA[indexKeyA++];

		if (swapNibbles) {
			temp = (((temp & 0x0F) << 4) | ((temp & 0xF0) >> 4));
		}

		if (!decrypt)
			temp ^= xorValue ^ keyB[indexKeyB++];
		else
			temp ^= keyA[indexKeyA++];

		data[x] = temp;

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

bool HatchVFS::ReadEntryData(VFSEntry* entry, Uint8* memory, size_t memSize) {
	size_t copyLength = entry->Size;
	if (copyLength > memSize) {
		copyLength = memSize;
	}

	StreamPtr->Seek(entry->Offset);
	StreamPtr->ReadBytes(memory, copyLength);

	// Decrypt it if it's encrypted
	if (entry->Flags & VFSE_ENCRYPTED) {
		Uint32 filenameHash = (int)strtol(entry->Name.c_str(), NULL, 16);

		CryptoXOR(memory, copyLength, filenameHash, true);
	}

	// Decompress it if it's compressed
	if (entry->Flags & VFSE_COMPRESSED) {
		Uint8* compressedMemory = (Uint8*)Memory::Malloc(entry->CompressedSize);
		if (!compressedMemory) {
			Memory::Free(memory);
			return false;
		}

		size_t compressedCopyLength = entry->CompressedSize;
		if (compressedCopyLength > entry->Size) {
			compressedCopyLength = entry->Size;
		}
		memcpy(compressedMemory, memory, compressedCopyLength);

		Uint8* srcData = memory;
		size_t srcSize = (size_t)entry->Size;
		Uint8* destData = compressedMemory;
		size_t destSize = (size_t)entry->CompressedSize;
		if (destSize > copyLength) {
			destSize = copyLength;
		}

		if (!ZLibStream::Decompress(srcData, srcSize, destData, destSize)) {
			Memory::Free(memory);
			return false;
		}

		Memory::Free(compressedMemory);
	}

	return true;
}

bool HatchVFS::PutFile(const char* filename, VFSEntry* entry) {
	if (ArchiveVFS::PutFile(filename, entry)) {
		NeedsRepacking = true;

		return true;
	}

	return false;
}

bool HatchVFS::EraseFile(const char* filename) {
	if (ArchiveVFS::EraseFile(filename)) {
		NeedsRepacking = true;

		return true;
	}

	return false;
}

Stream* HatchVFS::OpenMemStreamForEntry(VFSEntry* entry) {
	if (entry == nullptr) {
		return nullptr;
	}

	MemoryStream* memStream = MemoryStream::New(entry->Size);
	if (memStream == nullptr) {
		return nullptr;
	}

	Uint8* memory = memStream->pointer_start;

	// Read cached data if available
	if (entry->CachedData) {
		memcpy(memory, entry->CachedData, entry->Size);
	}
	// Else, read from file
	else if (!ReadEntryData(entry, memory, entry->Size)) {
		memStream->Close();

		return nullptr;
	}

	return (Stream*)memStream;
}

bool HatchVFS::Flush() {
	if (!NeedsRepacking) {
		return false;
	}

	bool success = false;

	// Calculate total length of all files and headers
	size_t totalSize = 10 + (32 * NumEntries);

	for (VFSEntryMap::iterator it = Entries.begin();
		it != Entries.end();
		it++) {
		totalSize += it->second->CompressedSize;
	}

	// Begin writing
	MemoryStream* out = MemoryStream::New(totalSize);
	if (out == nullptr) {
		Log::Print(Log::LOG_ERROR, "Could not open MemoryStream for packing HATCH file!");
		return false;
	}

	// Write magic
	out->WriteBytes(MAGIC_HATCH, MAGIC_HATCH_SIZE);

	// Write version
	out->WriteByte(0x01);
	out->WriteByte(0x00);
	out->WriteByte(0x00);

	// Write number of files
	out->WriteUInt16(NumEntries);

	size_t tocEnd = out->Position() + (32 * NumEntries);
	size_t offsetGLOB = 0;

	for (size_t i = 0; i < NumEntries; i++) {
		std::string entryName = EntryNames[i];
		VFSEntry* entry = Entries[entryName];

		Uint32 hash = (int)strtol(entryName.c_str(), NULL, 16);
		entry->Offset = tocEnd + offsetGLOB;

		bool shouldCopy = false;

		Uint8 dataType = 0;
		if (entry->Flags & VFSE_ENCRYPTED) {
			dataType = 2;
		}

		// Entry has cached data, but possibly no in-file representation
		if (entry->CachedData != nullptr) {
			bool shouldCompressOrEncrypt = (entry->Flags & (VFSE_COMPRESSED | VFSE_ENCRYPTED)) != 0;

			if (entry->CachedDataInFile) {
				Memory::Free(entry->CachedDataInFile);
				entry->CachedDataInFile = nullptr;
			}

			entry->CompressedSize = entry->Size;
			entry->CachedDataInFile = entry->CachedData;

			if (shouldCompressOrEncrypt) {
				if (entry->Flags & VFSE_COMPRESSED) {
					void* compressedData = nullptr;
					size_t compressedSize = 0;

					if (ZLibStream::Compress(entry->CachedData, entry->Size, &compressedData, &compressedSize)) {
						entry->CachedDataInFile = (Uint8 *)compressedData;
						entry->CompressedSize = compressedSize;
					}
					else {
						Log::Print(Log::LOG_ERROR, "Could not compress entry %s!", entry->Name.c_str());
					}
				}

				if (entry->Flags & VFSE_ENCRYPTED) {
					if (entry->CachedDataInFile == entry->CachedData) {
						entry->CachedDataInFile = (Uint8 *)Memory::Malloc(entry->CompressedSize);
						memcpy(entry->CachedDataInFile, entry->CachedData, entry->CompressedSize);
					}

					if (entry->CachedData != nullptr) {
						CryptoXOR(entry->CachedDataInFile, entry->CompressedSize, hash, false);
					}
					else {
						Log::Print(Log::LOG_ERROR, "Could not encrypt entry %s!", entry->Name.c_str());

						dataType = 0;
					}
				}
			}
		}
		// Entry should be copied
		else if (entry->CachedData == nullptr && entry->CachedDataInFile == nullptr) {
			shouldCopy = true;
		}

		// Write into the table of contents
		out->WriteUInt32(hash);
		out->WriteUInt64(entry->Offset);
		out->WriteUInt64(entry->Size);
		out->WriteUInt32(dataType);
		out->WriteUInt64(entry->CompressedSize);

		// Write the file data
		size_t lastPosition = out->Position();
		out->Seek(entry->Offset);
		if (shouldCopy) {
			StreamPtr->Seek(entry->Offset);
			StreamPtr->CopyTo(out, entry->Size);
		}
		else {
			out->WriteBytes(entry->CachedDataInFile, entry->CompressedSize);
		}
		out->Seek(lastPosition);

		offsetGLOB += entry->CompressedSize;

		entry->DeleteCache();
	}

	// Finally write to the .hatch file
	if (StreamPtr->MakeWritable(true)) {
		StreamPtr->Seek(0);

		size_t totalLength = tocEnd + offsetGLOB;

		out->Seek(0);
		out->CopyTo(StreamPtr, totalLength);

		if (!StreamPtr->MakeReadable(true)) {
			Log::Print(Log::LOG_ERROR, "Could not make HATCH file stream readable!");

			return false;
		}

		StreamPtr->Seek(0);

		NeedsRepacking = false;

		success = true;
	}
	else {
		Log::Print(Log::LOG_ERROR, "Could not make HATCH file stream writable!");
	}

	out->Close();

	return success;
}

void HatchVFS::Close() {
	if (NeedsRepacking) {
		Flush();
	}

	if (StreamPtr) {
		StreamPtr->Close();
		StreamPtr = nullptr;
	}

	ArchiveVFS::Close();
}

HatchVFS::~HatchVFS() {
	Close();
}
