#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>

class VertexBuffer {
public:
    VertexAttribute* Vertices;          // count = max vertex count
    FaceInfo*        FaceInfoBuffer;    // count = max face count
    Uint32           Capacity;
    Uint32           VertexCount;
    Uint32           FaceCount;
    Uint32           UnloadPolicy;
};
#endif

#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/Diagnostics/Memory.h>

PUBLIC STATIC VertexBuffer* VertexBuffer::Create(Uint32 maxVertices, int unloadPolicy) {
    VertexBuffer* buffer = new VertexBuffer;

    buffer->Capacity = 0;
    buffer->UnloadPolicy = unloadPolicy;

    VertexBuffer::Init(buffer, maxVertices);
    VertexBuffer::Clear(buffer);

    return buffer;
}
PUBLIC STATIC void          VertexBuffer::Init(VertexBuffer* buffer, Uint32 maxVertices) {
    Uint32 maxFaces = maxVertices / 3;

    if (!buffer->Capacity) {
        buffer->Vertices = (VertexAttribute*)Memory::Calloc(maxVertices, sizeof(VertexAttribute));
        buffer->FaceInfoBuffer = (FaceInfo*)Memory::Calloc(maxFaces, sizeof(FaceInfo));
    }
    else
        VertexBuffer::Resize(buffer, maxVertices, maxFaces);

    buffer->Capacity = maxVertices;
}
PUBLIC STATIC void          VertexBuffer::Clear(VertexBuffer* buffer) {
    buffer->VertexCount = 0;
    buffer->FaceCount = 0;

    memset(buffer->Vertices, 0x00, buffer->Capacity * sizeof(VertexAttribute));
}
PUBLIC STATIC void          VertexBuffer::Resize(VertexBuffer* buffer, Uint32 maxVertices, Uint32 maxFaces) {
    buffer->Vertices = (VertexAttribute*)Memory::Realloc(buffer->Vertices, maxVertices * sizeof(VertexAttribute));
    buffer->FaceInfoBuffer = (FaceInfo*)Memory::Realloc(buffer->FaceInfoBuffer, maxFaces * sizeof(FaceInfo));

    VertexBuffer::Clear(buffer);
}
PUBLIC STATIC void          VertexBuffer::Delete(VertexBuffer* buffer) {
    if (buffer->Capacity) {
        Memory::Free(buffer->Vertices);
        Memory::Free(buffer->FaceInfoBuffer);
    }

    delete buffer;
}

