#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Filesystem/VFS/EggVFS.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/IO/Compression/ZLibStream.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/Utilities/StringUtils.h>

#include <Libraries/miniz.h>

#define EGG_BLOCK_SIZE 512

#define EGG_OFFSET_NAME 0
#define EGG_OFFSET_FILESIZE 124
#define EGG_OFFSET_CHECKSUM 148
#define EGG_OFFSET_TYPE 156
#define EGG_OFFSET_USTAR_HEADER 257
#define EGG_OFFSET_PREFIX 345

#define EGG_LENGTH_NAME 100
#define EGG_LENGTH_FILESIZE 12
#define EGG_LENGTH_CHECKSUM 8
#define EGG_LENGTH_PREFIX 155

enum { EGG_HEADER_ERROR = -1, EGG_HEADER_OK = 0, EGG_HEADER_EMPTY = 1, EGG_HEADER_END = 2 };

#define GZIP_MAGIC "\x1F\x8B"

Uint64 EggVFS::MaxCompressedPreloadSize = EGG_BLOCK_SIZE * 1024; // 512KiB

bool EggVFS::Open(Stream* stream) {
	int type = EGG_COMPRESSION_TYPE_NONE;
	size_t startOffset = 0;

	Uint8 magic[2];
	stream->ReadBytes(magic, sizeof magic);

	if (memcmp(magic, GZIP_MAGIC, sizeof magic) == 0) {
		// It's a gzip file. Parse its header
		if (!ReadGzipHeader(stream)) {
			Log::Print(Log::LOG_ERROR, "Invalid gzip header!");
			return false;
		}

		InflateStream = Memory::Calloc(1, sizeof(z_stream));
		if (!InflateStream) {
			return false;
		}

		if (!InitInflateStream(InflateStream)) {
			Memory::Free(InflateStream);
			InflateStream = nullptr;
			return false;
		}

		type = EGG_COMPRESSION_TYPE_GZIP;
		startOffset = stream->Position();
	}

	return Open(stream, type, startOffset);
}

bool EggVFS::Open(Stream* stream, int type, size_t position) {
	z_stream* infstream = nullptr;

	if (type != EGG_COMPRESSION_TYPE_NONE) {
		if (!InflateStream) {
			InflateStream = Memory::Calloc(1, sizeof(z_stream));
			if (!InflateStream) {
				return false;
			}

			if (!InitInflateStream(InflateStream)) {
				Memory::Free(InflateStream);
				InflateStream = nullptr;
				return false;
			}
		}

		infstream = (z_stream*)InflateStream;
	}

	CompressionType = type;
	StreamPtr = stream;
	StartOffset = position;
	StreamPtr->Seek(StartOffset);

	int numEmptyRecords = 0;
	while (true) {
		int status = ReadHeader();
		if (status == EGG_HEADER_EMPTY) {
			numEmptyRecords++;

			// If there were two consecutive empty records, stop reading
			if (numEmptyRecords == 2) {
				break;
			}
		}
		else if (status == EGG_HEADER_END) {
			break;
		}
		else if (status == EGG_HEADER_ERROR) {
			return false;
		}
		else {
			numEmptyRecords = 0;
		}
	}

	Opened = true;

	return true;
}

bool EggVFS::IsFile(Stream* stream, int& compressionType) {
	size_t startOffset = stream->Position();

	z_stream inflateStream;
	inflateStream.zalloc = Z_NULL;
	inflateStream.zfree = Z_NULL;
	inflateStream.opaque = Z_NULL;

	char entryHeader[EGG_BLOCK_SIZE];

	Uint8 magic[2];
	stream->ReadBytes(magic, sizeof magic);

	// Check for gzip header
	if (memcmp(magic, GZIP_MAGIC, sizeof magic) == 0) {
		// Parse its header
		if (!ReadGzipHeader(stream)) {
			stream->Seek(startOffset);
			return false;
		}

		if (!InitInflateStream(&inflateStream)) {
			stream->Seek(startOffset);
			return false;
		}

		compressionType = EGG_COMPRESSION_TYPE_GZIP;
		startOffset = stream->Position();
	}
	else {
		compressionType = EGG_COMPRESSION_TYPE_NONE;
		stream->Seek(startOffset);
	}

	// Read entire header
	if (compressionType != EGG_COMPRESSION_TYPE_NONE) {
		char input[EGG_BLOCK_SIZE];
		stream->ReadBytes(input, sizeof input);

		inflateStream.next_in = (Bytef*)input;
		inflateStream.next_out = (Bytef*)entryHeader;
		inflateStream.avail_in = EGG_BLOCK_SIZE;
		inflateStream.avail_out = EGG_BLOCK_SIZE;

		int ret = inflate(&inflateStream, Z_NO_FLUSH);
		inflateEnd(&inflateStream);

		if (ret != Z_OK) {
			stream->Seek(startOffset);
			return false;
		}
	}
	else {
		stream->ReadBytes(entryHeader, sizeof entryHeader);
	}

	stream->Seek(startOffset);

	// Check for UStar header
	return memcmp(&entryHeader[EGG_OFFSET_USTAR_HEADER], "ustar\0", 6) == 0;
}

bool EggVFS::InitInflateStream(void* inflateStream) {
	int ret = inflateInit2((z_stream*)inflateStream, -Z_DEFAULT_WINDOW_BITS);
	if (ret != Z_OK) {
		Log::Print(Log::LOG_ERROR,
			"Could not decompress egg file! (%s)",
			mz_error(ret));
		inflateEnd((z_stream*)inflateStream);
		return false;
	}

	return true;
}

int EggVFS::ReadHeader() {
	char header[EGG_BLOCK_SIZE];

	char name[EGG_LENGTH_NAME];
	char prefix[EGG_LENGTH_PREFIX];

	memset(prefix, '\0', EGG_LENGTH_PREFIX);

	// Decompress data
	if (CompressionType != EGG_COMPRESSION_TYPE_NONE) {
		int ret = Decompress((Uint8*)header, EGG_BLOCK_SIZE);
		if (ret == Z_STREAM_END) {
			// End of stream
			return EGG_HEADER_END;
		}
		else if (ret != Z_OK) {
			Log::Print(Log::LOG_ERROR,
				"Could not decompress egg file! (%s)",
				mz_error(ret));
			return EGG_HEADER_ERROR;
		}
	}
	else {
		// Read entire header
		StreamPtr->ReadBytes(header, EGG_BLOCK_SIZE);
		StreamPosition += EGG_BLOCK_SIZE;
	}

	// Copy the name
	memcpy(name, &header[EGG_OFFSET_NAME], EGG_LENGTH_NAME);
	name[EGG_LENGTH_NAME - 1] = '\0';

	// Potentially empty record, skip
	if (name[0] == '\0') {
		return EGG_HEADER_EMPTY;
	}

	// Get the file size, and convert from octal to base 10
	char filesize[EGG_LENGTH_FILESIZE];
	size_t filesizeInBytes = 0;

	memcpy(filesize, &header[EGG_OFFSET_FILESIZE], EGG_LENGTH_FILESIZE);
	filesize[EGG_LENGTH_FILESIZE - 1] = '\0';
	StringUtils::OctalToBase10(&filesizeInBytes, filesize);

	// Get the checksum, and convert from octal to base 10
	char checksum[EGG_LENGTH_CHECKSUM];
	size_t checksumInBytes = 0;

	memcpy(checksum, &header[EGG_OFFSET_CHECKSUM], EGG_LENGTH_CHECKSUM);
	checksum[EGG_LENGTH_CHECKSUM - 1] = '\0';
	StringUtils::OctalToBase10(&checksumInBytes, checksum);

	// Check for UStar header
	if (memcmp(&header[EGG_OFFSET_USTAR_HEADER], "ustar\0", 6) == 0) {
		// Read the filename prefix
		memcpy(prefix, &header[EGG_OFFSET_PREFIX], EGG_LENGTH_PREFIX);
		checksum[EGG_LENGTH_PREFIX - 1] = '\0';
	}

	// Define the full filename
	std::string entryName = std::string(name);
	if (prefix[0] != '\0') {
		entryName = std::string(prefix) + "/" + entryName;
	}

	bool shouldLoad = false;

	// Get type flag
	char type = header[EGG_OFFSET_TYPE];
	if (type == '0') {
		if (StringUtils::StrCaseStr(entryName.c_str(), "GNUSparseFile.")) {
			Log::Print(Log::LOG_WARN,
				"Sparse files not supported, ignoring entry \"%s\"",
				entryName.c_str());
		}
		else {
			// It's a normal file
			shouldLoad = true;
		}
	}
	else if (type == 'x') {
		Log::Print(Log::LOG_WARN,
			"Extended headers not supported, ignoring entry \"%s\"",
			entryName.c_str());
	}
	else if (type == 'g') {
		Log::Print(Log::LOG_WARN,
			"Global extended headers not supported, ignoring entry \"%s\"",
			entryName.c_str());
	}
	else if (type == 'S') {
		Log::Print(Log::LOG_WARN,
			"Sparse headers not supported, ignoring entry \"%s\"",
			entryName.c_str());
	}

	size_t lastStreamPosition = StreamPosition;

	// Add the entry
	if (shouldLoad) {
		if (!LoadEntry(entryName, filesizeInBytes)) {
			return EGG_HEADER_ERROR;
		}

		// Calculate the checksum
		size_t calculatedChecksum = CalculateChecksum(header);
		if (calculatedChecksum != checksumInBytes) {
			// It's not an error if it doesn't match, but the entry might be corrupted.
			Log::Print(Log::LOG_WARN,
				"Entry \"%s\" has an invalid checksum!",
				entryName.c_str());
		}
	}

	// Get the offset of the next block
	size_t nextBlockOffset = GetNextBlockOffset(filesizeInBytes);
	if (nextBlockOffset == 0) {
		return EGG_HEADER_OK;
	}

	nextBlockOffset -= StreamPosition - lastStreamPosition;

	if (CompressionType != EGG_COMPRESSION_TYPE_NONE) {
		// Read the remaining bytes
		int ret = Decompress(nullptr, nextBlockOffset);
		if (ret != Z_OK) {
			Log::Print(Log::LOG_ERROR,
				"Could not decompress egg file! (%s)",
				mz_error(ret));
			return EGG_HEADER_ERROR;
		}
	}
	else {
		// Skip to the next record
		StreamPtr->Skip(nextBlockOffset);
		StreamPosition += nextBlockOffset;
	}

	return EGG_HEADER_OK;
}

bool EggVFS::LoadEntry(std::string name, size_t size) {
	VFSEntry* entry = new VFSEntry();
	entry->Name = name;
	entry->Offset = StreamPosition;
	entry->Size = entry->CompressedSize = size;
	entry->Flags = entry->FileFlags = 0;

	if (!ArchiveVFS::AddEntry(entry)) {
		delete entry;
		Log::Print(Log::LOG_WARN,
			"Egg file has duplicate entry \"%s\", ignoring",
			name.c_str());
		return true;
	}

	if (!ShouldPreloadEntry(entry)) {
		return true;
	}

	entry->CachedData = (Uint8*)Memory::Calloc(entry->Size, sizeof(Uint8));
	if (!entry->CachedData) {
		return false;
	}

	if (CompressionType != EGG_COMPRESSION_TYPE_NONE) {
		int ret = Decompress(entry->CachedData, entry->Size);
		if (ret != Z_OK) {
			Log::Print(Log::LOG_ERROR,
				"Could not decompress egg file! (%s)",
				mz_error(ret));
			return false;
		}
	}
	else {
		StreamPtr->ReadBytes(entry->CachedData, entry->Size);
		StreamPosition += entry->Size;
	}

	return true;
}

// Read a gzip header, except the GZIP_MAGIC bytes
bool EggVFS::ReadGzipHeader(Stream* stream) {
	// Compression Method (CM)
	Uint8 cm = stream->ReadByte();
	if (cm != 8) {
		// Must be Deflate
		return false;
	}

	// Flags (FLG)
	Uint8 flg = stream->ReadByte();

	// Timestamp (MTIME)
	stream->Skip(4);

	// Extra Flags (XFL)
	stream->Skip(1);

	// Operating System (OS)
	stream->Skip(1);

	// Now parse the flags to see if there is more data that needs to be skipped.
	// Skip extra fields if FEXTRA is set
	if (flg & (1 << 2)) {
		Uint16 xlen = stream->ReadUInt16();
		int remaining = xlen;
		while (remaining > 0) {
			// Read SI1 and SI2
			stream->Skip(2);
			remaining -= 2;

			// Size of the field
			Uint16 size = stream->ReadUInt16();
			remaining -= 2;

			stream->Skip(size);
			remaining -= size;
		}
	}

	// Skip filename if FNAME is set
	if (flg & (1 << 3)) {
		stream->SkipString();
	}

	// Skip file comment if FCOMMENT is set
	if (flg & (1 << 4)) {
		stream->SkipString();
	}

	// Skip CRC-32 if FHCRC is set
	if (flg & (1 << 1)) {
		stream->Skip(2);
	}

	// Deflate stream starts here.
	// There are additional fields after the stream, but we don't validate it for speed.
	return true;
}

// Handles seeking uncompressed and compressed streams
int EggVFS::Seek(size_t position) {
	if (CompressionType == EGG_COMPRESSION_TYPE_NONE) {
		StreamPtr->Seek(position);
		StreamPosition = position;
		return Z_OK;
	}

	if (position > StreamPosition) {
		return Decompress(nullptr, position - StreamPosition);
	}
	else if (position < StreamPosition) {
		z_stream* infstream = (z_stream*)InflateStream;
		int ret = inflateReset(infstream);
		if (ret != Z_OK) {
			return ret;
		}

		infstream->avail_in = 0;

		StreamPosition = 0;
		StreamPtr->Seek(StartOffset);

		return Decompress(nullptr, position);
	}

	return Z_OK;
}

// Decompress a Deflate stream
int EggVFS::Decompress(Uint8* out, size_t bytesToRead) {
	int ret = Z_OK;

	z_stream* infstream = (z_stream*)InflateStream;
	infstream->total_out = 0;

	do {
		if (infstream->avail_in == 0) {
			size_t read =
				StreamPtr->ReadBytes(InputBuffer, EGG_DECOMPRESSION_BUFFER_SIZE);
			if (read == 0) {
				break;
			}

			infstream->next_in = (Bytef*)InputBuffer;
			infstream->avail_in = read;
		}

		size_t current = infstream->total_out;
		size_t remaining = bytesToRead - infstream->total_out;
		infstream->next_out = (Bytef*)BlockBuffer;
		infstream->avail_out =
			remaining > EGG_BLOCK_BUFFER_SIZE ? EGG_BLOCK_BUFFER_SIZE : remaining;

		ret = inflate(infstream, Z_NO_FLUSH);
		if (ret != Z_OK && ret != Z_STREAM_END) {
			break;
		}

		size_t read = infstream->total_out - current;
		StreamPosition += read;

		if (out) {
			memcpy(out + current, BlockBuffer, read);
		}

		if (ret == Z_STREAM_END) {
			break;
		}
	} while (infstream->total_out < bytesToRead);

	return ret;
}

// Calculate the checksum of a header, except the checksum bytes
unsigned int EggVFS::CalculateChecksum(char* header) {
	unsigned int checksum = 0;

	// Fill checksum field with spaces
	memset(&header[EGG_OFFSET_CHECKSUM], ' ', EGG_LENGTH_CHECKSUM);

	for (size_t i = 0; i < EGG_BLOCK_SIZE; i++) {
		checksum += header[i];
	}

	return checksum;
}

// Get an offset to the next block
size_t EggVFS::GetNextBlockOffset(size_t size) {
	size_t remainder = size % EGG_BLOCK_SIZE;
	if (remainder) {
		return size + EGG_BLOCK_SIZE - (size % EGG_BLOCK_SIZE);
	}
	else {
		return size;
	}
}

bool EggVFS::ShouldPreloadEntry(VFSEntry* entry) {
	if (entry->Size == 0) {
		return false;
	}

	if (Flags & VFS_PRELOAD) {
		return true;
	}

	if (CompressionType != EGG_COMPRESSION_TYPE_NONE && MaxCompressedPreloadSize > 0) {
		return entry->Size <= MaxCompressedPreloadSize;
	}

	return false;
}

// Read the data of an entry, possibly decompressing it
bool EggVFS::ReadEntryData(VFSEntry* entry, Uint8* memory, size_t memSize) {
	size_t copyLength = entry->Size;
	if (copyLength > memSize) {
		copyLength = memSize;
	}

	// Nothing to read
	if (copyLength == 0) {
		return true;
	}

	// Seek to the expected position
	int ret = Seek(entry->Offset);
	if (CompressionType != EGG_COMPRESSION_TYPE_NONE) {
		if (ret != Z_OK) {
			Log::Print(
				Log::LOG_ERROR, "Could not seek in egg file! (%s)", mz_error(ret));
			return false;
		}

		// Decompress the entry
		ret = Decompress(memory, copyLength);
		if (ret != Z_OK && ret != Z_STREAM_END) {
			Log::Print(Log::LOG_ERROR,
				"Could not decompress egg file! (%s)",
				mz_error(ret));
			return false;
		}
	}
	else {
		// Read the entry
		StreamPtr->ReadBytes(memory, copyLength);
		StreamPosition += copyLength;
	}

	return true;
}

bool EggVFS::PreloadFiles(std::vector<std::string> list) {
	// Read all entries sequentially
	for (size_t i = 0; i < NumEntries; i++) {
		std::string entryName = EntryNames[i];
		if (std::find(list.begin(), list.end(), entryName) == list.end()) {
			continue;
		}

		VFSEntry* entry = Entries[entryName];
		if (entry->CachedData) {
			// Already loaded
			continue;
		}

		Log::Print(Log::LOG_VERBOSE, "Preloading %s...", entryName.c_str());

		// Allocate entry data
		entry->CachedData = (Uint8*)Memory::Calloc(entry->Size, sizeof(Uint8));
		if (!entry->CachedData) {
			Log::Print(Log::LOG_ERROR, "Could not preload %s!", entryName.c_str());
			return false;
		}

		// Seek to the expected position
		int ret = Seek(entry->Offset);
		if (CompressionType != EGG_COMPRESSION_TYPE_NONE) {
			if (ret != Z_OK) {
				Log::Print(Log::LOG_ERROR,
					"Could not preload %s! (%s)",
					entryName.c_str(),
					mz_error(ret));
				Memory::Free(entry->CachedData);
				return false;
			}
		}

		if (CompressionType != EGG_COMPRESSION_TYPE_NONE) {
			// Decompress the entry
			int ret = Decompress(entry->CachedData, entry->Size);
			if (ret != Z_OK && ret != Z_STREAM_END) {
				Log::Print(Log::LOG_ERROR,
					"Could not preload %s! (%s)",
					entryName.c_str(),
					mz_error(ret));
				Memory::Free(entry->CachedData);
				return false;
			}
		}
		else {
			// Read the entry
			StreamPtr->ReadBytes(entry->CachedData, entry->Size);
			StreamPosition += entry->Size;
		}
	}

	return true;
}

void EggVFS::Close() {
	if (InflateStream) {
		inflateEnd((z_stream*)InflateStream);
		Memory::Free(InflateStream);
		InflateStream = nullptr;
	}

	ArchiveVFS::Close();
}

EggVFS::~EggVFS() {
	Close();
}
