#include <Engine/IO/Compression/CompressionEnums.h>
#include <Engine/IO/Compression/ZLibStream.h>
#include <Engine/IO/MemoryStream.h>

#undef min
#undef max

#define MINIZ_HEADER_FILE_ONLY
#include <Libraries/miniz.h>

// TODO: Implement

ZLibStream* ZLibStream::New(Stream* other_stream, Uint8 mode) {
	ZLibStream* stream = new ZLibStream;
	if (!stream) {
		return NULL;
	}

	if (!other_stream) {
		goto FREE;
	}

	stream->other_stream = other_stream;
	stream->mode = mode;

	if (mode == CompressionMode::DECOMPRESS) {
		size_t inSize = other_stream->Length() - 4;
		void* inMem = Memory::Malloc(inSize);
		stream->memory_size = other_stream->ReadUInt32BE();
		stream->memory = (Uint8*)Memory::Malloc(stream->memory_size);

		other_stream->ReadBytes(inMem, inSize);
		stream->Decompress(inMem, inSize);

		Memory::Free(inMem);
	}

	return stream;

FREE:
	delete stream;
	return NULL;
}

bool ZLibStream::IsReadable() {
	return mode == CompressionMode::DECOMPRESS;
}
bool ZLibStream::IsWritable() {
	return mode == CompressionMode::COMPRESS;
}
bool ZLibStream::MakeReadable(bool readable) {
	return readable == IsReadable();
}
bool ZLibStream::MakeWritable(bool writable) {
	return writable == IsWritable();
}

void ZLibStream::Close() {
	if (memory_start) {
		Memory::Free(memory_start);
		memory_size = 0;
		memory_start = memory = nullptr;
	}

	Stream::Close();
}
void ZLibStream::Seek(Sint64 offset) {}
void ZLibStream::SeekEnd(Sint64 offset) {}
void ZLibStream::Skip(Sint64 offset) {}
size_t ZLibStream::Position() {
	return 0;
}
size_t ZLibStream::Length() {
	return 0;
}

size_t ZLibStream::ReadBytes(void* data, size_t n) {
	if (IsReadable()) {
		memcpy(data, memory, n);
		return n;
	}

	return 0;
}

size_t ZLibStream::WriteBytes(void* data, size_t n) {
	if (IsReadable()) {
		memcpy(memory, data, n);
		memory += n;
		return n;
	}

	return 0;
}

bool ZLibStream::Decompress(void* dst, size_t dstLen, void* src, size_t srcLen) {
	z_stream infstream;
	infstream.zalloc = Z_NULL;
	infstream.zfree = Z_NULL;
	infstream.opaque = Z_NULL;

	infstream.next_in = (Bytef*)src;
	infstream.next_out = (Bytef*)dst;
	infstream.avail_in = (int)srcLen;
	infstream.avail_out = (int)dstLen;

	inflateInit(&infstream);
	inflate(&infstream, Z_NO_FLUSH);
	inflateEnd(&infstream);

	// TODO: Check for error
	return true;
}

bool ZLibStream::Decompress(void* in, size_t inLen) {
	z_stream infstream;
	infstream.zalloc = Z_NULL;
	infstream.zfree = Z_NULL;
	infstream.opaque = Z_NULL;

	infstream.next_in = (Bytef*)in;
	infstream.next_out = (Bytef*)memory;
	infstream.avail_in = inLen;
	infstream.avail_out = memory_size;

	inflateInit(&infstream);
	inflate(&infstream, Z_NO_FLUSH);
	inflateEnd(&infstream);

	// TODO: Check for error
	return true;
}

#define CHUNK_SIZE 16384

bool ZLibStream::Compress(void* src, size_t srcLen, void** dst, size_t* dstLen) {
	MemoryStream* outStream = MemoryStream::New(CHUNK_SIZE);
	if (outStream == nullptr) {
		return false;
	}

	Uint8 chunk[CHUNK_SIZE];

	z_stream defstream;
	defstream.zalloc = Z_NULL;
	defstream.zfree = Z_NULL;
	defstream.opaque = Z_NULL;

	deflateInit(&defstream, Z_DEFAULT_COMPRESSION);

	defstream.next_in = (Bytef*)src;
	defstream.avail_in = (int)srcLen;

	do {
		defstream.avail_out = CHUNK_SIZE;
		defstream.next_out = (Bytef*)chunk;

		int ret = deflate(&defstream, Z_FINISH);
		if (ret == Z_STREAM_ERROR) {
			outStream->Close();
			deflateEnd(&defstream);
			return false;
		}

		outStream->WriteBytes(chunk, CHUNK_SIZE - defstream.avail_out);
	} while (defstream.avail_out == 0);

	*dst = outStream->pointer_start;
	*dstLen = outStream->Length();
	outStream->owns_memory = false;
	outStream->Close();

	deflateEnd(&defstream);

	return true;
}
