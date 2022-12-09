#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>

class VertexBuffer {
public:
    VertexAttribute* Vertices       = nullptr; // count = max vertex count
    FaceInfo*        FaceInfoBuffer = nullptr; // count = max face count
    Uint32           Capacity       = 0;
    Uint32           VertexCount    = 0;
    Uint32           FaceCount      = 0;
    Uint32           UnloadPolicy;
};
#endif

#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/Diagnostics/Memory.h>

PUBLIC               VertexBuffer::VertexBuffer() {

}
PUBLIC               VertexBuffer::VertexBuffer(Uint32 numVertices, int unloadPolicy) {
    UnloadPolicy = unloadPolicy;

    Init(numVertices);
    Clear();
}
PUBLIC void          VertexBuffer::Init(Uint32 numVertices) {
    if (!numVertices)
        numVertices = 192;

    if (Capacity != numVertices)
        Resize(numVertices);
}
PUBLIC void          VertexBuffer::Clear() {
    VertexCount = 0;
    FaceCount = 0;

    // Not sure why we do this
    memset(Vertices, 0x00, Capacity * sizeof(VertexAttribute));
}
PUBLIC void          VertexBuffer::Resize(Uint32 numVertices) {
    if (!numVertices)
        numVertices = 192;

    Capacity = numVertices;
    Vertices = (VertexAttribute*)Memory::Realloc(Vertices, numVertices * sizeof(VertexAttribute));
    FaceInfoBuffer = (FaceInfo*)Memory::Realloc(FaceInfoBuffer, ((numVertices / 3) + 1) * sizeof(FaceInfo));
}
PUBLIC               VertexBuffer::~VertexBuffer() {
    Memory::Free(Vertices);
    Memory::Free(FaceInfoBuffer);
}
