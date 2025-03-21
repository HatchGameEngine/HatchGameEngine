#ifndef ENGINE_IO_MEMORYSTREAM_H
#define ENGINE_IO_MEMORYSTREAM_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>

class MemoryStream : public Stream {
private:
	bool Writable = true;

public:
	Uint8* pointer = NULL;
	Uint8* pointer_start = NULL;
	size_t size = 0;
	bool owns_memory = false;

	static MemoryStream* New(size_t size);
	static MemoryStream* New(Stream* other);
	static MemoryStream* New(void* data, size_t size);
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
	Uint32 ReadCompressed(void* out);
	Uint32 ReadCompressed(void* out, size_t outSz);
	size_t WriteBytes(void* data, size_t n);
};

#endif /* ENGINE_IO_MEMORYSTREAM_H */
