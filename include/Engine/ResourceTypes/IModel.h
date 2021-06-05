#ifndef ENGINE_RESOURCETYPES_IMODEL_H
#define ENGINE_RESOURCETYPES_IMODEL_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class Material;

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Graphics.h>
#include <Engine/IO/Stream.h>

class IModel {
public:
    Vector3*   PositionBuffer;
    Vector2*   UVBuffer;
    Uint32*    ColorBuffer;
    Uint16     VertexCount;
    Uint16     FrameCount;
    Uint16     MeshCount;
    Mesh*      Meshes;
    Uint16     TotalVertexIndexCount;
    Uint8      VertexFlag;
    Uint8      FaceVertexCount;
    Material** Materials;
    Uint8      MaterialCount;

    IModel();
    IModel(const char* filename);
    bool Load(Stream* stream, const char* filename);
    bool ReadRSDK(Stream* stream);
    bool HasColors();
    void Cleanup();
};

#endif /* ENGINE_RESOURCETYPES_IMODEL_H */
