#if INTERFACE
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

    Sint16**   VertexIndexBuffer;
    Uint16*    VertexIndexCount;
    Uint16     TotalVertexIndexCount;

    Uint8      VertexFlag;
    Uint8      FaceVertexCount;

    Material** Materials;
};
#endif

#include <Engine/ResourceTypes/IModel.h>

#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/ModelFormats/COLLADAReader.h>
#include <Engine/Utilities/StringUtils.h>

PUBLIC IModel::IModel() {

}
PUBLIC IModel::IModel(const char* filename) {
    ResourceStream* resourceStream = ResourceStream::New(filename);
    if (!resourceStream) return;

    this->Load(resourceStream, filename);

    if (resourceStream) resourceStream->Close();
}
PUBLIC bool IModel::Load(Stream* stream, const char* filename) {
    if (!stream) return false;
    if (!filename) return false;

    if (StringUtils::StrCaseStr(filename, ".dae"))
        return !!COLLADAReader::Convert(this, stream);

    return this->ReadRSDK(stream);
}

PUBLIC bool IModel::ReadRSDK(Stream* stream) {
    if (stream->ReadUInt32BE() != 0x4D444C00) { // MDL0
        Log::Print(Log::LOG_ERROR, "Model not of RSDK type!");
        return false;
    }

    MeshCount = 1;
    Materials = nullptr;

    VertexFlag = stream->ReadByte();
    FaceVertexCount = stream->ReadByte();
    VertexCount = stream->ReadUInt16();
    FrameCount = stream->ReadUInt16();

    if (VertexFlag & VertexType_Normal)
        PositionBuffer = (Vector3*)Memory::Malloc(VertexCount * FrameCount * 2 * sizeof(Vector3));
    else
        PositionBuffer = (Vector3*)Memory::Malloc(VertexCount * FrameCount * sizeof(Vector3));

    if (VertexFlag & VertexType_UV)
        UVBuffer = (Vector2*)Memory::Malloc(VertexCount * FrameCount * sizeof(Vector2));

    if (VertexFlag & VertexType_Color)
        ColorBuffer = (Uint32*)Memory::Malloc(VertexCount * FrameCount * sizeof(Uint32));

    // Read UVs
    if (VertexFlag & VertexType_UV) {
        int uvX, uvY;
        for (int i = 0; i < VertexCount; i++) {
            Vector2* uv = &UVBuffer[i];
            uv->X = uvX = (int)(stream->ReadFloat() * 0x10000);
            uv->Y = uvY = (int)(stream->ReadFloat() * 0x10000);
            // Copy the values to other frames
            for (int f = 1; f < FrameCount; f++) {
                uv += VertexCount;
                uv->X = uvX;
                uv->Y = uvY;
            }
        }
    }
    // Read Colors
    if (VertexFlag & VertexType_Color) {
        Uint32* colorPtr, color;
        for (int i = 0; i < VertexCount; i++) {
            colorPtr = &ColorBuffer[i];
            *colorPtr = color = stream->ReadUInt32();
            // Copy the value to other frames
            for (int f = 1; f < FrameCount; f++) {
                colorPtr += VertexCount;
                *colorPtr = color;
            }
        }
    }

    TotalVertexIndexCount = stream->ReadInt16();
    VertexIndexCount = (Uint16*)Memory::Malloc(MeshCount * sizeof(Uint16));
    VertexIndexCount[0] = TotalVertexIndexCount;

    VertexIndexBuffer = (Sint16**)Memory::Malloc(MeshCount * sizeof(Sint16*));
    VertexIndexBuffer[0] = (Sint16*)Memory::Malloc((TotalVertexIndexCount + 1) * sizeof(Sint16));

    for (int i = 0; i < TotalVertexIndexCount; i++) {
        VertexIndexBuffer[0][i] = stream->ReadInt16();
    }
    VertexIndexBuffer[0][TotalVertexIndexCount] = -1;

    if (VertexFlag & VertexType_Normal) {
        Vector3* vert = &PositionBuffer[0];
        int totalVertexCount = VertexCount * FrameCount;
        for (int v = 0; v < totalVertexCount; v++) {
            vert->X = (int)(stream->ReadFloat() * 0x100);
            vert->Y = (int)(stream->ReadFloat() * 0x100);
            vert->Z = (int)(stream->ReadFloat() * 0x100);
            vert++;

            vert->X = (int)(stream->ReadFloat() * 0x10000);
            vert->Y = (int)(stream->ReadFloat() * 0x10000);
            vert->Z = (int)(stream->ReadFloat() * 0x10000);
            vert++;
        }
    }
    else {
        Vector3* vert = &PositionBuffer[0];
        int totalVertexCount = VertexCount * FrameCount;
        for (int v = 0; v < totalVertexCount; v++) {
            vert->X = (int)(stream->ReadFloat() * 0x100);
            vert->Y = (int)(stream->ReadFloat() * 0x100);
            vert->Z = (int)(stream->ReadFloat() * 0x100);
            vert++;
        }
    }

    return true;
}
PUBLIC bool IModel::HasColors() {
    return false; // Colors.size() > 0;
}
PUBLIC void IModel::Cleanup() {
    // Does nothing
    // Graphics::DeleteBufferID(BufferID_V);
    // Graphics::DeleteBufferID(BufferID_N);
}
