#ifndef ENGINE_IO_COMPRESSION_ZLIBSTREAM_H
#define ENGINE_IO_COMPRESSION_ZLIBSTREAM_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>

class ZLibStream : public Stream {
private:
	bool Decompress(void* in, size_t inLen);

public:
	Stream* other_stream = NULL;
	Uint8 mode = 0;
	Uint8* memory = NULL;
	Uint8* memory_start = NULL;
	size_t memory_size = 0;

	static ZLibStream* New(Stream* other_stream, Uint8 mode);
	bool IsReadable();
	bool IsWritable();
	bool MakeReadable(bool readable);
	bool MakeWritable(bool writable);
	void Close();
	void Seek(Sint64 offset);
	void SeekEnd(Sint64 offset);
	void Skip(Sint64 offset);
	size_t Position();
	size_t Length();
	size_t ReadBytes(void* data, size_t n);
	size_t WriteBytes(void* data, size_t n);
	static bool Decompress(void* dst, size_t dstLen, void* src, size_t srcLen);
	static bool Compress(void* src, size_t srcLen, void** dst, size_t* dstLen);
};

#endif /* ENGINE_IO_COMPRESSION_ZLIBSTREAM_H */
