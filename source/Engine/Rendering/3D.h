#ifndef I3D_H
#define I3D_H

struct Vector2 {
    int X;
    int Y;
};
struct Vector3 {
    int X;
    int Y;
    int Z;
};
struct Matrix4x4i {
    int Column[4][4];
};
enum VertexType {
    VertexType_Position = 0,
    VertexType_Normal = 1,
    VertexType_UV = 2,
    VertexType_Color = 4,
};
#define MAX_VECTOR_COUNT 0x800
#define MAX_ARRAY_BUFFERS 0x20

struct VertexWeightInfo {
    int  Influences;
    int *JointIndices;
    int *WeightIndices;
};

struct Mesh {
    Sint16*    VertexIndexBuffer;
    Uint16     VertexIndexCount;

    Uint8      VertexFlag;

    Uint8*     FaceMaterials;
};

struct VertexAttribute {
    Vector3 Position;
    Vector3 Normal;
    Vector2 UV;
    Uint32  Color;
};
struct FaceInfo {
    void* Material;
    int   Depth;
    int   VerticesStartIndex;
};
struct ArrayBuffer {
    VertexAttribute* VertexBuffer;      // count = max vertex count
    FaceInfo*        FaceInfoBuffer;    // count = max face count
    Uint8*           FaceSizeBuffer;    // count = max face count
    Uint32           PerspectiveBitshiftX;
    Uint32           PerspectiveBitshiftY;
    Uint32           LightingAmbientR;
    Uint32           LightingAmbientG;
    Uint32           LightingAmbientB;
    Uint32           LightingDiffuseR;
    Uint32           LightingDiffuseG;
    Uint32           LightingDiffuseB;
    Uint32           LightingSpecularR;
    Uint32           LightingSpecularG;
    Uint32           LightingSpecularB;
    Uint16           VertexCapacity;
    Uint16           VertexCount;
    Uint16           FaceCount;
    Uint8            DrawMode;
    bool             Initialized;
};

#endif /* I3D_H */
