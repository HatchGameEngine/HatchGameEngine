#ifndef ENGINE_IO_FILESTREAM_H
#define ENGINE_IO_FILESTREAM_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>

class FileStream : public Stream {
private:
	std::string Filename;
	Uint32 CurrentAccess;
	bool UseURLs;

	static Stream* OpenFile(const char* filename, Uint32 access, bool allowURLs);
	bool Reopen(Uint32 newAccess);

public:
	Stream* StreamPtr;
	enum {
		// Read access to a file in the current directory.
		READ_ACCESS,

		// Write access a file in the current directory.
		WRITE_ACCESS,

		// Append access a file in the current directory.
		// Note that the underlying stream might not support this
		// access mode, so use with caution.
		APPEND_ACCESS
	};

	static FileStream* New(const char* filename, Uint32 access);
	static FileStream* New(const char* filename, Uint32 access, bool allowURLs);
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

#endif /* ENGINE_IO_FILESTREAM_H */
