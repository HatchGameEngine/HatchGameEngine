#ifndef ENGINE_IO_TEXTSTREAM_H
#define ENGINE_IO_TEXTSTREAM_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>

class TextStream : public Stream {
public:
	Uint8* pointer = NULL;
	Uint8* pointer_start = NULL;
	size_t size = 0;

	static TextStream* New(Stream* other);
	static TextStream* New(const char* text);
	static TextStream* New(std::string text);
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

#endif /* ENGINE_IO_TEXTSTREAM_H */
