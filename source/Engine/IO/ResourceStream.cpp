#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>
class ResourceStream : public Stream {
public:
    Uint8* pointer = NULL;
    Uint8* pointer_start = NULL;
    size_t size = 0;
};
#endif

#include <Engine/IO/ResourceStream.h>

#include <Engine/ResourceTypes/ResourceManager.h>

PUBLIC STATIC ResourceStream* ResourceStream::New(const char* filename) {
    ResourceStream* stream = new (std::nothrow) ResourceStream;
    if (!stream) {
        return NULL;
    }

    if (!filename)
        goto FREE;

    if (!ResourceManager::LoadResource(filename, &stream->pointer_start, &stream->size))
        goto FREE;

    stream->pointer = stream->pointer_start;

    return stream;

    FREE:
        delete stream;
        return NULL;
}

PUBLIC        void            ResourceStream::Close() {
    Memory::Free(pointer_start);
    Stream::Close();
}
PUBLIC        void            ResourceStream::Seek(Sint64 offset) {
    pointer = pointer_start + offset;
}
PUBLIC        void            ResourceStream::SeekEnd(Sint64 offset) {
    pointer = pointer_start + size + offset;
}
PUBLIC        void            ResourceStream::Skip(Sint64 offset) {
    pointer = pointer + offset;
}
PUBLIC        size_t          ResourceStream::Position() {
    return pointer - pointer_start;
}
PUBLIC        size_t          ResourceStream::Length() {
    return size;
}

PUBLIC        size_t          ResourceStream::ReadBytes(void* data, size_t n) {
    if (n > size - Position()) {
        n = size - Position();
    }
    if (n == 0) return 0;

    memcpy(data, pointer, n);
    pointer += n;
    return n;
}
PUBLIC        size_t          ResourceStream::WriteBytes(void* data, size_t n) {
    // Cannot write to a resource.
    return 0;
}
