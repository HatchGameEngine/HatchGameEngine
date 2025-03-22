#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/Compression/ZLibStream.h>
#include <Engine/IO/Stream.h>

#define READ_TYPE_MACRO(type) \
	type data = {}; \
	ReadBytes(&data, sizeof(data));

bool Stream::IsReadable() {
	return false;
}
bool Stream::IsWritable() {
	return false;
}
bool Stream::MakeReadable(bool readable) {
	return false;
}
bool Stream::MakeWritable(bool writable) {
	return false;
}

void Stream::Close() {
	delete this;
}
void Stream::Seek(Sint64 offset) {}
void Stream::SeekEnd(Sint64 offset) {}
void Stream::Skip(Sint64 offset) {}
size_t Stream::Position() {
	return 0;
}
size_t Stream::Length() {
	return 0;
}

size_t Stream::ReadBytes(void* data, size_t n) {
	if (n == 0) {
		return 0;
	}

#if DEBUG
	if (Position() + n > Length()) {
		Log::Print(Log::LOG_ERROR, "Attempted to read past stream.");
	}
#endif

	return 0;
}
Uint8 Stream::ReadByte() {
	READ_TYPE_MACRO(Uint8);
	return data;
}
Uint16 Stream::ReadUInt16() {
	READ_TYPE_MACRO(Uint16);
	return FROM_LE16(data);
}
Uint16 Stream::ReadUInt16BE() {
	READ_TYPE_MACRO(Uint16);
	return FROM_BE16(data);
}
Uint32 Stream::ReadUInt32() {
	READ_TYPE_MACRO(Uint32);
	return FROM_LE32(data);
}
Uint32 Stream::ReadUInt32BE() {
	READ_TYPE_MACRO(Uint32);
	return FROM_BE32(data);
}
Uint64 Stream::ReadUInt64() {
	READ_TYPE_MACRO(Uint64);
	return FROM_LE64(data);
}
Sint16 Stream::ReadInt16() {
	READ_TYPE_MACRO(Sint16);
	return FROM_LE16(data);
}
Sint16 Stream::ReadInt16BE() {
	READ_TYPE_MACRO(Sint16);
	return FROM_BE16(data);
}
Sint32 Stream::ReadInt32() {
	READ_TYPE_MACRO(Sint32);
	return FROM_LE32(data);
}
Sint32 Stream::ReadInt32BE() {
	READ_TYPE_MACRO(Sint32);
	return FROM_BE32(data);
}
Sint64 Stream::ReadInt64() {
	READ_TYPE_MACRO(Sint64);
	return FROM_LE64(data);
}
float Stream::ReadFloat() {
	READ_TYPE_MACRO(float);
	return FROM_LE32F(data);
}
char* Stream::ReadLine() {
	Uint8 byte = 0;
	size_t start = Position();
	while ((byte = ReadByte()) != '\n' && byte)
		;

	size_t size = Position() - start;

	char* data = (char*)Memory::TrackedMalloc("Stream::ReadLine", size + 1);

	if (size > 0) {
		Skip(-size);
		ReadBytes(data, size);
	}

	data[size] = 0;

	return data;
}
char* Stream::ReadString() {
	size_t start = Position();
	while (ReadByte())
		;

	size_t size = Position() - start;

	char* data = (char*)Memory::TrackedMalloc("Stream::ReadString", size + 1);

	if (size > 0) {
		Skip(-size);
		ReadBytes(data, size);
	}

	data[size] = 0;

	return data;
}
void Stream::SkipString() {
	while (ReadByte())
		;
}
Uint16* Stream::ReadUnicodeString() {
	size_t start = Position();
	while (ReadUInt16())
		;

	Uint16* data;

	size_t size = Position() - start;
	if (size == 0) {
		data = (Uint16*)Memory::TrackedCalloc(
			"Stream::ReadUnicodeString", 1, sizeof(Uint16));
	}
	else {
		data = (Uint16*)Memory::TrackedMalloc("Stream::ReadUnicodeString", size);
		Skip(-size);
		ReadBytes(data, size);
	}

	return data;
}
char* Stream::ReadHeaderedString() {
	Uint8 size = ReadByte();

	char* data = (char*)Memory::TrackedMalloc("Stream::ReadHeaderedString", size + 1);
	if (size > 0) {
		ReadBytes(data, size);
	}
	data[size] = 0;

	return data;
}
Uint32 Stream::ReadCompressed(void* out) {
	Uint32 compressed_size = ReadUInt32() - 4;
	Uint32 uncompressed_size = ReadUInt32BE();

	void* buffer = Memory::Malloc(compressed_size);
	ReadBytes(buffer, compressed_size);

	ZLibStream::Decompress(out, uncompressed_size, buffer, compressed_size);
	Memory::Free(buffer);

	return uncompressed_size;
}
Uint32 Stream::ReadCompressed(void* out, size_t outSz) {
	Uint32 compressed_size = ReadUInt32() - 4;
	ReadUInt32BE(); // uncompressed_size

	void* buffer = Memory::Malloc(compressed_size);
	ReadBytes(buffer, compressed_size);

	ZLibStream::Decompress(out, outSz, buffer, compressed_size);
	Memory::Free(buffer);

	return (Uint32)outSz;
}

size_t Stream::WriteBytes(void* data, size_t n) {
	return 0;
}
void Stream::WriteByte(Uint8 data) {
	WriteBytes(&data, sizeof(data));
}
void Stream::WriteUInt16(Uint16 data) {
	data = TO_LE16(data);
	WriteBytes(&data, sizeof(data));
}
void Stream::WriteUInt16BE(Uint16 data) {
	data = TO_BE16(data);
	WriteBytes(&data, sizeof(data));
}
void Stream::WriteUInt32(Uint32 data) {
	data = TO_LE32(data);
	WriteBytes(&data, sizeof(data));
}
void Stream::WriteUInt32BE(Uint32 data) {
	data = TO_BE32(data);
	WriteBytes(&data, sizeof(data));
}
void Stream::WriteUInt64(Uint64 data) {
	data = TO_LE64(data);
	WriteBytes(&data, sizeof(data));
}
void Stream::WriteInt16(Sint16 data) {
	data = TO_LE16(data);
	WriteBytes(&data, sizeof(data));
}
void Stream::WriteInt16BE(Sint16 data) {
	WriteUInt16BE((Uint16)data);
}
void Stream::WriteInt32(Sint32 data) {
	data = TO_LE32(data);
	WriteBytes(&data, sizeof(data));
}
void Stream::WriteInt32BE(Sint32 data) {
	WriteUInt32BE((Sint32)data);
}
void Stream::WriteInt64(Sint64 data) {
	data = TO_LE64(data);
	WriteBytes(&data, sizeof(data));
}
void Stream::WriteFloat(float data) {
	data = TO_LE32F(data);
	WriteBytes(&data, sizeof(data));
}
void Stream::WriteString(const char* string) {
	size_t size = strlen(string) + 1;
	WriteBytes((void*)string, size);
}
void Stream::WriteHeaderedString(const char* string) {
	size_t size = strlen(string) + 1;
	WriteByte((Uint8)size);
	WriteBytes((void*)string, size);
}

size_t Stream::CopyTo(Stream* dest, size_t n) {
	void* memory = Memory::Malloc(n);

	ReadBytes(memory, n);
	size_t written = dest->WriteBytes(memory, n);

	Memory::Free(memory);

	return written;
}
size_t Stream::CopyTo(Stream* dest) {
	size_t length = Length() - Position();

	return CopyTo(dest, length);
}

Stream::~Stream() {}
