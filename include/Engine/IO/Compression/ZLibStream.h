#ifndef ENGINE_IO_COMPRESSION_ZLIBSTREAM_H
#define ENGINE_IO_COMPRESSION_ZLIBSTREAM_H

#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>

class ZLibStream : public Stream {
private:
    void Decompress(void* in, size_t inLen);

public:
    Stream* other_stream = NULL;
    uint32_t mode = 0;
    void* memory = NULL;
    size_t memory_size = 0;

    static ZLibStream* New(Stream* other_stream, uint32_t mode);
    void Close();
    void Seek(Sint64 offset);
    void SeekEnd(Sint64 offset);
    void Skip(Sint64 offset);
    size_t Position();
    size_t Length();
    size_t ReadBytes(void* data, size_t n);
    size_t WriteBytes(void* data, size_t n);
    static void Decompress(void* dst, size_t dstLen, void* src, size_t srcLen);
};

#endif /* ENGINE_IO_COMPRESSION_ZLIBSTREAM_H */
