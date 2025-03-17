#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/SDLStream.h>

SDL_RWops* SDLStream::OpenFile(const char* filename, Uint32 access) {
	const char* accessString = nullptr;

	switch (access) {
	case SDLStream::READ_ACCESS:
		accessString = "rb";
		break;
	case SDLStream::WRITE_ACCESS:
		accessString = "wb";
		break;
	case SDLStream::APPEND_ACCESS:
		accessString = "ab";
		break;
	}

	SDL_RWops* rw = SDL_RWFromFile(filename, accessString);
	if (rw == nullptr) {
		return nullptr;
	}

	if (access == SDLStream::READ_ACCESS) {
		Sint64 rwSize = SDL_RWsize(rw);
		if (rwSize < 0) {
			Log::Print(Log::LOG_ERROR,
				"Could not get size of file \"%s\": %s",
				filename,
				SDL_GetError());
			SDL_RWclose(rw);
			return nullptr;
		}
	}

	return rw;
}

SDLStream* SDLStream::New(const char* filename, Uint32 access) {
	SDLStream* stream = new SDLStream;
	if (!stream) {
		return NULL;
	}

	stream->f = OpenFile(filename, access);
	if (!stream->f) {
		goto FREE;
	}

	stream->Filename = std::string(filename);
	stream->CurrentAccess = access;

	return stream;

FREE:
	delete stream;
	return NULL;
}

bool SDLStream::Reopen(Uint32 newAccess) {
	if (CurrentAccess == newAccess) {
		return true;
	}

	SDL_RWops* newFile = OpenFile(Filename.c_str(), newAccess);
	if (!newFile) {
		return false;
	}

	SDL_RWclose(f);
	f = newFile;

	CurrentAccess = newAccess;

	return true;
}

bool SDLStream::IsReadable() {
	return CurrentAccess == SDLStream::READ_ACCESS;
}
bool SDLStream::IsWritable() {
	return !IsReadable();
}
bool SDLStream::MakeReadable(bool readable) {
	if (!readable) {
		return MakeWritable(true);
	}

	return Reopen(READ_ACCESS);
}
bool SDLStream::MakeWritable(bool writable) {
	if (!writable) {
		return MakeReadable(true);
	}

	return Reopen(WRITE_ACCESS);
}

void SDLStream::Close() {
	SDL_RWclose(f);
	f = NULL;
	Stream::Close();
}
void SDLStream::Seek(Sint64 offset) {
	SDL_RWseek(f, offset, RW_SEEK_SET);
}
void SDLStream::SeekEnd(Sint64 offset) {
	SDL_RWseek(f, offset, RW_SEEK_END);
}
void SDLStream::Skip(Sint64 offset) {
	SDL_RWseek(f, offset, RW_SEEK_CUR);
}
size_t SDLStream::Position() {
	return SDL_RWtell(f);
}
size_t SDLStream::Length() {
	return SDL_RWsize(f);
}

size_t SDLStream::ReadBytes(void* data, size_t n) {
	// if (!f) Log::Print(Log::LOG_ERROR, "Attempt to read from
	// closed stream.")
	return SDL_RWread(f, data, 1, n);
}

size_t SDLStream::WriteBytes(void* data, size_t n) {
	return SDL_RWwrite(f, data, 1, n);
}
