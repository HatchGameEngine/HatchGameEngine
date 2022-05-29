#ifndef I3D_H
#define I3D_H

#include <Engine/Math/Matrix4x4.h>

struct Vector2 {
    int X;
    int Y;
};
struct Vector3 {
    Sint64 X;
    Sint64 Y;
    Sint64 Z;
};
struct Vector4 {
    Sint64 X;
    Sint64 Y;
    Sint64 Z;
    Sint64 W;
};
enum VertexType {
    VertexType_Position = 0,
    VertexType_Normal = 1,
    VertexType_UV = 2,
    VertexType_Color = 4,
};

#define MAX_ARRAY_BUFFERS 0x20
#define MAX_VERTEX_BUFFERS 256
#define MAX_POLYGON_VERTICES 16
#define NUM_FRUSTUM_PLANES 6

struct VertexWeightInfo {
    int  Influences;
    int* JointIndices;
    int* WeightIndices;
};

struct Mesh {
    Sint16*    VertexIndexBuffer;
    Uint16     VertexIndexCount;
    Uint8      VertexFlag;
    Uint8*     FaceMaterials;
};

struct VertexAttribute {
    Vector4 Position;
    Vector4 Normal;
    Vector2 UV;
    Uint32  Color;
};
struct FaceMaterial {
    Uint32 Specular[4];
    Uint32 Ambient[4];
    Uint32 Diffuse[4];
    void*  Texture;
};
struct FaceInfo {
    int          NumVertices;
    int          VerticesStartIndex;
    bool         UseMaterial;
    FaceMaterial Material;
    Uint8        Opacity;
    Uint8        BlendFlag;
    int          Depth;
};
struct Frustum {
    Vector4 Plane;
    Vector4 Normal;
};
struct PolygonClipBuffer {
    VertexAttribute Buffer[MAX_POLYGON_VERTICES];
    int             NumPoints;
    int             MaxPoints;
};

#endif /* I3D_H */
