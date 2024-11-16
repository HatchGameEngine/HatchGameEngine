#include <Engine/IO/SDLStream.h>

SDLStream* SDLStream::New(const char* filename, Uint32 access) {
    SDLStream* stream = new SDLStream;
    if (!stream) {
        return NULL;
    }

    const char* accessString = NULL;

    switch (access) {
        case SDLStream::READ_ACCESS: accessString = "rb"; break;
        case SDLStream::WRITE_ACCESS: accessString = "wb"; break;
        case SDLStream::APPEND_ACCESS: accessString = "ab"; break;
    }

    stream->f = SDL_RWFromFile(filename, accessString);
    if (!stream->f)
        goto FREE;

    return stream;

    FREE:
        delete stream;
        return NULL;
}

void        SDLStream::Close() {
    SDL_RWclose(f);
    f = NULL;
    Stream::Close();
}
void        SDLStream::Seek(Sint64 offset) {
    SDL_RWseek(f, offset, RW_SEEK_SET);
}
void        SDLStream::SeekEnd(Sint64 offset) {
    SDL_RWseek(f, offset, RW_SEEK_END);
}
void        SDLStream::Skip(Sint64 offset) {
    SDL_RWseek(f, offset, RW_SEEK_CUR);
}
size_t      SDLStream::Position() {
    return SDL_RWtell(f);
}
size_t      SDLStream::Length() {
    return SDL_RWsize(f);
}

size_t      SDLStream::ReadBytes(void* data, size_t n) {
    // if (!f) Log::Print(Log::LOG_ERROR, "Attempt to read from closed stream.")
    return SDL_RWread(f, data, 1, n);
}

size_t      SDLStream::WriteBytes(void* data, size_t n) {
    return SDL_RWwrite(f, data, 1, n);
}
