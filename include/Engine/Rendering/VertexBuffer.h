#ifndef ENGINE_RENDERING_VERTEXBUFFER_H
#define ENGINE_RENDERING_VERTEXBUFFER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


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

    static VertexBuffer* Create(Uint32 maxVertices, int unloadPolicy);
    static void          Init(VertexBuffer* buffer, Uint32 maxVertices);
    static void          Clear(VertexBuffer* buffer);
    static void          Resize(VertexBuffer* buffer, Uint32 maxVertices, Uint32 maxFaces);
    static void          Delete(VertexBuffer* buffer);
};

#endif /* ENGINE_RENDERING_VERTEXBUFFER_H */
