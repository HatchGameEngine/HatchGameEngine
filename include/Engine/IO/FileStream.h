#ifndef ENGINE_IO_FILESTREAM_H
#define ENGINE_IO_FILESTREAM_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>

class FileStream : public Stream {
private:
	std::string Filename;
	Uint32 CurrentAccess;

	static FILE* OpenFile(const char* filename, Uint32 access);
	bool Reopen(Uint32 newAccess);

public:
	FILE* f;
	size_t size;
	enum {
		READ_ACCESS = 0,
		WRITE_ACCESS = 1,
		APPEND_ACCESS = 2,
		SAVEGAME_ACCESS = 16,
		PREFERENCES_ACCESS = 32,
	};

	static FileStream* New(const char* filename, Uint32 access);
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
