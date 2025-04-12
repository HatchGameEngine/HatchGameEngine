#include <Engine/Filesystem/VFS/HatchVFS.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/IO/Compression/ZLibStream.h>
#include <Engine/IO/MemoryStream.h>

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
		entry->Flags = entry->FileFlags = entryFlags;
		entry->CompressedSize = compressedSize;

		if (!ArchiveVFS::AddEntry(entry)) {
			Log::Print(Log::LOG_WARN,
				"HATCH file has duplicate entry \"%s\", ignoring",
				entryName);
		}
	}

	StreamPtr = stream;
	Opened = true;

	return true;
}

std::string HatchVFS::TransformFilename(const char* filename) {
	char transformedFilename[ENTRY_NAME_LENGTH + 1];

	Uint32 crc32 = CRC32::EncryptString(filename);

	snprintf(transformedFilename, sizeof transformedFilename, "%08x", crc32);

	return std::string(transformedFilename);
}

bool HatchVFS::SupportsCompression() {
	return true;
}
bool HatchVFS::SupportsEncryption() {
	return true;
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

		if (decrypt) {
			temp ^= xorValue ^ keyB[indexKeyB++];
		}
		else {
			temp ^= keyA[indexKeyA++];
		}

		if (swapNibbles) {
			temp = (((temp & 0x0F) << 4) | ((temp & 0xF0) >> 4));
		}

		if (!decrypt) {
			temp ^= xorValue ^ keyB[indexKeyB++];
		}
		else {
			temp ^= keyA[indexKeyA++];
		}

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

bool HatchVFS::ReadEntryData(VFSEntry &entry, Uint8 *memory, size_t memSize){
	size_t copyLength = entry.Size;
	if (copyLength > memSize) {
		copyLength = memSize;
	}

	StreamPtr->Seek(entry.Offset);
	StreamPtr->ReadBytes(memory, copyLength);

	// Decrypt it if it's encrypted
	if (entry.Flags & VFSE_ENCRYPTED) {
		Uint32 filenameHash = (int)strtol(entry.Name.c_str(), NULL, 16);

		CryptoXOR(memory, copyLength, filenameHash, true);
	}

	// Decompress it if it's compressed
	if (entry.Flags & VFSE_COMPRESSED) {
		Uint8* compressedMemory = (Uint8*)Memory::Malloc(entry.CompressedSize);
		if (!compressedMemory) {
			Memory::Free(&memory);
			return false;
		}

		size_t compressedCopyLength = entry.CompressedSize;
		if (compressedCopyLength > entry.Size) {
			compressedCopyLength = entry.Size;
		}
		memcpy(compressedMemory, memory, compressedCopyLength);

		Uint8* srcData = memory;
		size_t srcSize = (size_t)entry.Size;
		Uint8* destData = compressedMemory;
		size_t destSize = (size_t)entry.CompressedSize;
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

VFSEnumeration HatchVFS::EnumerateFiles(const char* path) {
	if (path != nullptr && path[0] != '\0') {
		VFSEnumeration enumeration;
		enumeration.Result = VFSEnumerationResult::CANNOT_ENUMERATE_RELATIVELY;
		return enumeration;
	}

	return ArchiveVFS::EnumerateFiles(path);
}

Stream* HatchVFS::OpenMemStreamForEntry(VFSEntry &entry) {
	MemoryStream* memStream = MemoryStream::New(entry.Size);
	if (memStream == nullptr) {
		return nullptr;
	}

	Uint8* memory = memStream->pointer_start;

	// Read cached data if available
	if (entry.CachedData) {
		memcpy(memory, entry.CachedData, entry.Size);
	}
	// Else, read from file
	else if (!ReadEntryData(entry, memory, entry.Size)) {
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

	for (VFSEntryMap::iterator it = Entries.begin(); it != Entries.end(); it++) {
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

		Uint8* cachedData = entry->CachedData;
		Uint8* cachedDataInFile = entry->CachedDataInFile;

		bool shouldFreeData = false;
		bool shouldFreeDataInFile = false;

		bool needsRewrite = entry->Flags != entry->FileFlags;
		if (needsRewrite && cachedData == nullptr) {
			Uint8* memory = (Uint8*)Memory::Malloc(entry->Size);
			if (!memory) {
				Log::Print(Log::LOG_ERROR,
					"Could not allocate memory for entry %s!",
					entry->Name.c_str());
				out->Close();
				return false;
			}

			if (!ReadEntryData(*entry, memory, entry->Size)) {
				Log::Print(Log::LOG_ERROR,
					"Could not read entry %s!",
					entry->Name.c_str());
				out->Close();
				return false;
			}

			cachedData = memory;
			shouldFreeData = true;
		}

		// The only flag that matters.
		Uint8 dataType = (entry->Flags & VFSE_ENCRYPTED) ? 2 : 0;

		size_t compressedSize = entry->Size;

		// Entry needs to be rewritten
		if (cachedData != nullptr) {
			// cachedDataInFile = cachedData, as if it was not compressed or encrypted
			cachedDataInFile = cachedData;

			if (entry->Flags & VFSE_COMPRESSED) {
				void* compressed = nullptr;
				size_t compSize = 0;

				if (ZLibStream::Compress(
					    cachedData, entry->Size, &compressed, &compSize)) {
					cachedDataInFile = (Uint8*)compressed;
					compressedSize = compSize;
					shouldFreeDataInFile = true;
				}
				else {
					Log::Print(Log::LOG_ERROR,
						"Could not compress entry %s!",
						entry->Name.c_str());
				}
			}

			if (entry->Flags & VFSE_ENCRYPTED) {
				bool didEncrypt = false;

				Uint8* bufferToEncrypt = (Uint8*)Memory::Malloc(compressedSize);
				if (bufferToEncrypt != nullptr) {
					memcpy(bufferToEncrypt, cachedDataInFile, compressedSize);

					CryptoXOR(bufferToEncrypt, compressedSize, hash, false);

					// They are the same, so allocate its own memory.
					if (cachedDataInFile == cachedData) {
						cachedDataInFile =
							(Uint8*)Memory::Malloc(compressedSize);
						if (cachedDataInFile != nullptr) {
							shouldFreeDataInFile = true;
							didEncrypt = true;
						}
					}
					else {
						didEncrypt = true;
					}

					if (didEncrypt) {
						memcpy(cachedDataInFile,
							bufferToEncrypt,
							compressedSize);
					}

					Memory::Free(bufferToEncrypt);
				}

				if (!didEncrypt) {
					// Reset data flag so that the entry is not saved as encrypted
					dataType = 0;

					Log::Print(Log::LOG_ERROR,
						"Could not encrypt entry %s!",
						entry->Name.c_str());
				}
			}
		}

		size_t entryOffset = tocEnd + offsetGLOB;

		// Write into the table of contents
		out->WriteUInt32(hash);
		out->WriteUInt64(entryOffset);
		out->WriteUInt64(entry->Size);
		out->WriteUInt32(dataType);
		out->WriteUInt64(compressedSize);

		// Write the file data
		size_t lastPosition = out->Position();
		out->Seek(entryOffset);

		// Entry wasn't modified, so copy directly from the .hatch file
		if (cachedData == nullptr && cachedDataInFile == nullptr) {
			// Note that this should be the entry's old offset,
			// not the current one that will be written.
			StreamPtr->Seek(entry->Offset);
			StreamPtr->CopyTo(out, entry->Size);
		}
		else {
			// Write the data that was created for this entry.
			out->WriteBytes(cachedDataInFile, compressedSize);
		}

		out->Seek(lastPosition);

		if (shouldFreeData) {
			Memory::Free(cachedData);
		}
		if (shouldFreeDataInFile) {
			Memory::Free(cachedDataInFile);
		}

		offsetGLOB += compressedSize;
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

	// Actually commit the changes to the in-memory structures if everything went okay.
	if (success) {
		offsetGLOB = 0;

		for (size_t i = 0; i < NumEntries; i++) {
			VFSEntry* entry = Entries[EntryNames[i]];

			entry->FileFlags = entry->Flags;
			entry->Offset = tocEnd + offsetGLOB;

			offsetGLOB += entry->CompressedSize;

			entry->DeleteCache();
		}
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
