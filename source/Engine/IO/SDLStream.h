#ifndef ENGINE_IO_SDLSTREAM_H
#define ENGINE_IO_SDLSTREAM_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>

class SDLStream : public Stream {
private:
	std::string Filename;
	Uint32 CurrentAccess;

	static SDL_RWops* OpenFile(const char* filename, Uint32 access);
	bool Reopen(Uint32 newAccess);

public:
	SDL_RWops* f;
	enum { READ_ACCESS, WRITE_ACCESS, APPEND_ACCESS };

	static SDLStream* New(const char* filename, Uint32 access);
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

#endif /* ENGINE_IO_SDLSTREAM_H */
