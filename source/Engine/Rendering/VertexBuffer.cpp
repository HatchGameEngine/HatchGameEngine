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

PUBLIC               VertexBuffer::VertexBuffer() {
    Vertices = nullptr;
    FaceInfoBuffer = nullptr;
    VertexCount = 0;
    FaceCount = 0;
    Capacity = 0;
}
PUBLIC               VertexBuffer::VertexBuffer(Uint32 maxVertices, int unloadPolicy) {
    Capacity = 0;
    UnloadPolicy = unloadPolicy;

    Init(maxVertices);
    Clear();
}
PUBLIC void          VertexBuffer::Init(Uint32 maxVertices) {
    Uint32 maxFaces = maxVertices / 3;

    if (!Capacity) {
        Vertices = (VertexAttribute*)Memory::Calloc(maxVertices, sizeof(VertexAttribute));
        FaceInfoBuffer = (FaceInfo*)Memory::Calloc(maxFaces, sizeof(FaceInfo));
    }
    else
        Resize(maxVertices, maxFaces);

    Capacity = maxVertices;
}
PUBLIC void          VertexBuffer::Clear() {
    VertexCount = 0;
    FaceCount = 0;

    memset(Vertices, 0x00, Capacity * sizeof(VertexAttribute));
}
PUBLIC void          VertexBuffer::Resize(Uint32 maxVertices, Uint32 maxFaces) {
    Vertices = (VertexAttribute*)Memory::Realloc(Vertices, maxVertices * sizeof(VertexAttribute));
    FaceInfoBuffer = (FaceInfo*)Memory::Realloc(FaceInfoBuffer, maxFaces * sizeof(FaceInfo));

    Clear();
}
PUBLIC               VertexBuffer::~VertexBuffer() {
    if (Capacity) {
        Memory::Free(Vertices);
        Memory::Free(FaceInfoBuffer);
    }
}
