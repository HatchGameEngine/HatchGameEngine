#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>
class MemoryStream : public Stream {
public:
    Uint8* pointer = NULL;
    Uint8* pointer_start = NULL;
    size_t         size = 0;
    bool   owns_memory = false;
};
#endif

#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/Compression/ZLibStream.h>

PUBLIC STATIC MemoryStream* MemoryStream::New(size_t size) {
    void* data = Memory::Malloc(size);
    MemoryStream* stream = MemoryStream::New(data, size);
    if (!stream) {
        Memory::Free(data);
    }
    else {
        stream->owns_memory = true;
    }
    return stream;
}
PUBLIC STATIC MemoryStream* MemoryStream::New(Stream* other) {
    MemoryStream* stream = MemoryStream::New(other->Length());
    if (stream) {
        other->CopyTo(stream);
        stream->Seek(0);
    }
    return stream;
}
PUBLIC STATIC MemoryStream* MemoryStream::New(void* data, size_t size) {
    MemoryStream* stream = new (std::nothrow) MemoryStream;
    if (!stream) {
        return NULL;
    }

    if (!data)
        goto FREE;

    stream->pointer_start = (Uint8*)data;
    stream->pointer = stream->pointer_start;
    stream->size = size;

    return stream;

    FREE:
        delete stream;
        return NULL;
}

PUBLIC        void          MemoryStream::Close() {
    if (owns_memory)
        Memory::Free(pointer_start);

    Stream::Close();
}
PUBLIC        void          MemoryStream::Seek(Sint64 offset) {
    pointer = pointer_start + offset;
}
PUBLIC        void          MemoryStream::SeekEnd(Sint64 offset) {
    pointer = pointer_start + size + offset;
}
PUBLIC        void          MemoryStream::Skip(Sint64 offset) {
    pointer = pointer + offset;
}
PUBLIC        size_t        MemoryStream::Position() {
    return pointer - pointer_start;
}
PUBLIC        size_t        MemoryStream::Length() {
    return size;
}

PUBLIC        size_t        MemoryStream::ReadBytes(void* data, size_t n) {
    if (n > size - Position()) {
        n = size - Position();
    }
    if (n == 0) return 0;

    memcpy(data, pointer, n);
    pointer += n;
    return n;
}
PUBLIC        Uint32        MemoryStream::ReadCompressed(void* out) {
    Uint32 compressed_size = ReadUInt32() - 4;
    Uint32 uncompressed_size = ReadUInt32BE();

    ZLibStream::Decompress(out, uncompressed_size, pointer, compressed_size);
    pointer += compressed_size;

    return uncompressed_size;
}
PUBLIC        Uint32        MemoryStream::ReadCompressed(void* out, size_t outSz) {
    Uint32 compressed_size = ReadUInt32() - 4;
	ReadUInt32BE(); // Uint32 uncompressed_size = ReadUInt32BE();

    ZLibStream::Decompress(out, outSz, pointer, compressed_size);
    pointer += compressed_size;

    return (Uint32)outSz;
}

PUBLIC        size_t        MemoryStream::WriteBytes(void* data, size_t n) {
    if (Position() + n > size) {
        size_t pos = Position();
        pointer_start = (unsigned char*)Memory::Realloc(pointer_start, pos + n);
        pointer = pointer_start + pos;
    }
    memcpy(pointer, data, n);
    pointer += n;
    return n;
}
