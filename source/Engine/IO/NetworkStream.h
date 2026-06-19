#ifndef ENGINE_IO_NETWORKSTREAM_H
#define ENGINE_IO_NETWORKSTREAM_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>

class NetworkStream : public Stream {
public:
	enum {
		SERVER_SOCKET = 0,
		CLIENT_SOCKET = 1,
	};

	static NetworkStream* New(const char* filename, Uint32 access);
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
};

#endif /* ENGINE_IO_NETWORKSTREAM_H */
