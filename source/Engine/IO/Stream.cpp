#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Diagnostics/Memory.h>
class Stream {
public:
};
#endif

#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/Stream.h>
#include <Engine/IO/Compression/ZLibStream.h>

#define READ_TYPE_MACRO(type) \
    type data; \
    ReadBytes(&data, sizeof(data));

PUBLIC VIRTUAL void    Stream::Close() {
    delete this;
}
PUBLIC VIRTUAL void    Stream::Seek(Sint64 offset) {
}
PUBLIC VIRTUAL void    Stream::SeekEnd(Sint64 offset) {
}
PUBLIC VIRTUAL void    Stream::Skip(Sint64 offset) {
}
PUBLIC VIRTUAL size_t  Stream::Position() {
    return 0;
}
PUBLIC VIRTUAL size_t  Stream::Length() {
    return 0;
}

PUBLIC VIRTUAL size_t  Stream::ReadBytes(void* data, size_t n) {
#if DEBUG
    if (Position() + n > Length()) {
        Log::Print(Log::LOG_ERROR, "Attempted to read past stream.");
        assert(false);
    }
#endif
    return 0;
}
PUBLIC         Uint8   Stream::ReadByte() {
    READ_TYPE_MACRO(Uint8);
    return data;
}
PUBLIC         Uint16  Stream::ReadUInt16() {
    READ_TYPE_MACRO(Uint16);
    return data;
}
PUBLIC         Uint16  Stream::ReadUInt16BE() {
    return (Uint16)(ReadByte() << 8 | ReadByte());
}
PUBLIC         Uint32  Stream::ReadUInt32() {
    READ_TYPE_MACRO(Uint32);
    return data;
}
PUBLIC         Uint32  Stream::ReadUInt32BE() {
    return (Uint32)(ReadByte() << 24 | ReadByte() << 16 | ReadByte() << 8 | ReadByte());
}
PUBLIC         Uint64  Stream::ReadUInt64() {
    READ_TYPE_MACRO(Uint64);
    return data;
}
PUBLIC         Sint16  Stream::ReadInt16() {
    READ_TYPE_MACRO(Sint16);
    return data;
}
PUBLIC         Sint16  Stream::ReadInt16BE() {
    return (Sint16)(ReadByte() << 8 | ReadByte());
}
PUBLIC         Sint32  Stream::ReadInt32() {
    READ_TYPE_MACRO(Sint32);
    return data;
}
PUBLIC         Sint32  Stream::ReadInt32BE() {
    return (Sint32)(ReadByte() << 24 | ReadByte() << 16 | ReadByte() << 8 | ReadByte());
}
PUBLIC         Sint64  Stream::ReadInt64() {
    READ_TYPE_MACRO(Sint64);
    return data;
}
PUBLIC         float   Stream::ReadFloat() {
    READ_TYPE_MACRO(float);
    return data;
}
PUBLIC         char*   Stream::ReadLine() {
    Uint8 byte = 0;
    size_t start = Position();
    while ((byte = ReadByte()) != '\n' && byte);

    size_t size = Position() - start;

    char* data = (char*)Memory::TrackedMalloc("Stream::ReadLine", size + 1);
    Skip(-size);
    ReadBytes(data, size);
    data[size] = 0;

    return data;
}
PUBLIC         char*   Stream::ReadString() {
    size_t start = Position();
    while (ReadByte());

    size_t size = Position() - start;

    char* data = (char*)Memory::TrackedMalloc("Stream::ReadString", size + 1);
    Skip(-size);
    ReadBytes(data, size);
    data[size] = 0;

    return data;
}
PUBLIC         void    Stream::SkipString() {
    while (ReadByte());
}
PUBLIC         Uint16* Stream::ReadUnicodeString() {
    size_t start = Position();
    while (ReadUInt16());

    size_t size = Position() - start;

    Uint16* data = (Uint16*)Memory::TrackedMalloc("Stream::ReadUnicodeString", size);
    Skip(-size);
    ReadBytes(data, size);

    return data;
}
PUBLIC         char*   Stream::ReadHeaderedString() {
    Uint8 size = ReadByte();

    char* data = (char*)Memory::TrackedMalloc("Stream::ReadHeaderedString", size + 1);
    ReadBytes(data, size);
    data[size] = 0;

    return data;
}
PUBLIC VIRTUAL Uint32  Stream::ReadCompressed(void* out) {
    Uint32 compressed_size = ReadUInt32() - 4;
    Uint32 uncompressed_size = ReadUInt32BE();

    void*  buffer = Memory::Malloc(compressed_size);
    ReadBytes(buffer, compressed_size);

    ZLibStream::Decompress(out, uncompressed_size, buffer, compressed_size);
    Memory::Free(buffer);

    return uncompressed_size;
}
PUBLIC VIRTUAL Uint32  Stream::ReadCompressed(void* out, size_t outSz) {
    Uint32 compressed_size = ReadUInt32() - 4;
    ReadUInt32BE(); // uncompressed_size

    void*  buffer = Memory::Malloc(compressed_size);
    ReadBytes(buffer, compressed_size);

    ZLibStream::Decompress(out, outSz, buffer, compressed_size);
    Memory::Free(buffer);

    return (Uint32)outSz;
}

PUBLIC VIRTUAL size_t  Stream::WriteBytes(void* data, size_t n) {
    return 0;
}
PUBLIC         void    Stream::WriteByte(Uint8 data) {
    WriteBytes(&data, sizeof(data));
}
PUBLIC         void    Stream::WriteUInt16(Uint16 data) {
    WriteBytes(&data, sizeof(data));
}
PUBLIC         void    Stream::WriteUInt16BE(Uint16 data) {
    WriteByte(data >> 8 & 0xFF);
    WriteByte(data & 0xFF);
}
PUBLIC         void    Stream::WriteUInt32(Uint32 data) {
    WriteBytes(&data, sizeof(data));
}
PUBLIC         void    Stream::WriteUInt32BE(Uint32 data) {
    WriteByte(data >> 24 & 0xFF);
    WriteByte(data >> 16 & 0xFF);
    WriteByte(data >> 8 & 0xFF);
    WriteByte(data & 0xFF);
}
PUBLIC         void    Stream::WriteUInt64(Uint64 data) {
    WriteBytes(&data, sizeof(data));
}
PUBLIC         void    Stream::WriteInt16(Sint16 data) {
    WriteBytes(&data, sizeof(data));
}
PUBLIC         void    Stream::WriteInt16BE(Sint16 data) {
    WriteUInt16BE((Uint16)data);
}
PUBLIC         void    Stream::WriteInt32(Sint32 data) {
    WriteBytes(&data, sizeof(data));
}
PUBLIC         void    Stream::WriteInt32BE(Sint32 data) {
    WriteUInt32BE((Sint32)data);
}
PUBLIC         void    Stream::WriteInt64(Sint64 data) {
    WriteBytes(&data, sizeof(data));
}
PUBLIC         void    Stream::WriteFloat(float data) {
    WriteBytes(&data, sizeof(data));
}
PUBLIC         void    Stream::WriteString(const char* string) {
    size_t size = strlen(string) + 1;
    WriteBytes((void*)string, size);
}
PUBLIC         void    Stream::WriteHeaderedString(const char* string) {
    size_t size = strlen(string) + 1;
    WriteByte((Uint8)size);
    WriteBytes((void*)string, size);
}

PUBLIC         void    Stream::CopyTo(Stream* dest) {
    size_t length = Length() - Position();
    void*  memory = Memory::Malloc(length);

    // Seek(0);
    ReadBytes(memory, length);
    dest->WriteBytes(memory, length);
    // dest->Seek(0);

    Memory::Free(memory);
}

PUBLIC VIRTUAL         Stream::~Stream() {
}
