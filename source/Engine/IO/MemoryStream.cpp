#include <Engine/IO/Compression/ZLibStream.h>
#include <Engine/IO/MemoryStream.h>

MemoryStream* MemoryStream::New(size_t size) {
	void* data = Memory::Malloc(size);
	MemoryStream* stream = MemoryStream::New(data, size);
	if (!stream) {
		Memory::Free(data);
	}
	else {
		stream->owns_memory = true;
	}
	return stream;
}
MemoryStream* MemoryStream::New(Stream* other) {
	MemoryStream* stream = MemoryStream::New(other->Length());
	if (stream) {
		other->CopyTo(stream);
		stream->Seek(0);
	}
	return stream;
}
MemoryStream* MemoryStream::New(void* data, size_t size) {
	MemoryStream* stream = nullptr;
	if (!data) {
		return nullptr;
	}

	stream = new (std::nothrow) MemoryStream;
	if (!stream) {
		return nullptr;
	}

	stream->pointer_start = (Uint8*)data;
	stream->pointer = stream->pointer_start;
	stream->size = size;

	return stream;
}

bool MemoryStream::IsReadable() {
	return true;
}
bool MemoryStream::IsWritable() {
	return Writable;
}
bool MemoryStream::MakeReadable(bool readable) {
	return true;
}
bool MemoryStream::MakeWritable(bool writable) {
	Writable = writable;

	return true;
}

void MemoryStream::Close() {
	if (owns_memory) {
		Memory::Free(pointer_start);
	}

	Stream::Close();
}
void MemoryStream::Seek(Sint64 offset) {
	pointer = pointer_start + offset;
}
void MemoryStream::SeekEnd(Sint64 offset) {
	pointer = pointer_start + size + offset;
}
void MemoryStream::Skip(Sint64 offset) {
	pointer = pointer + offset;
}
size_t MemoryStream::Position() {
	return pointer - pointer_start;
}
size_t MemoryStream::Length() {
	return size;
}

size_t MemoryStream::ReadBytes(void* data, size_t n) {
	if (n > size - Position()) {
		n = size - Position();
	}
	if (n == 0) {
		return 0;
	}

	memcpy(data, pointer, n);
	pointer += n;
	return n;
}
Uint32 MemoryStream::ReadCompressed(void* out) {
	Uint32 compressed_size = ReadUInt32() - 4;
	Uint32 uncompressed_size = ReadUInt32BE();

	ZLibStream::Decompress(out, uncompressed_size, pointer, compressed_size);
	pointer += compressed_size;

	return uncompressed_size;
}
Uint32 MemoryStream::ReadCompressed(void* out, size_t outSz) {
	Uint32 compressed_size = ReadUInt32() - 4;
	ReadUInt32BE();

	ZLibStream::Decompress(out, outSz, pointer, compressed_size);
	pointer += compressed_size;

	return (Uint32)outSz;
}

size_t MemoryStream::WriteBytes(void* data, size_t n) {
	// For speed, this doesn't check IsWritable().
	size_t pos = Position();
	if (pos + n > size) {
		size = pos + n;
		pointer_start = (unsigned char*)Memory::Realloc(pointer_start, size);
		pointer = pointer_start + pos;
	}
	memcpy(pointer, data, n);
	pointer += n;
	return n;
}
