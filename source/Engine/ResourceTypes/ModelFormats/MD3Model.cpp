#if INTERFACE
#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/IModel.h>
class MD3Model {
public:
    static Sint32 Version;
    static Sint32 DataEndOffset;
    static vector<string> MaterialNames;
};
#endif

#include <Engine/ResourceTypes/ModelFormats/MD3Model.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Mesh.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Diagnostics/Log.h>

#define MD3_MODEL_MAGIC 0x49445033 // IDP3

#define MD3_VERSION 15

#define MD3_XYZ_SCALE (1.0 / 64)

#define MD3_MAX_TAGS 16
#define MD3_MAX_SURFACES 32
#define MD3_MAX_FRAMES 1024
#define MD3_MAX_SHADERS 256
#define MAX_QPATH 64

Sint32 MD3Model::Version;
Sint32 MD3Model::DataEndOffset;
vector<string> MD3Model::MaterialNames;

PUBLIC STATIC bool MD3Model::IsMagic(Stream* stream) {
    Uint32 magic = stream->ReadUInt32BE();

    stream->Skip(-4);

    return magic == MD3_MODEL_MAGIC;
}

PUBLIC STATIC void MD3Model::DecodeNormal(Uint16 index, float& x, float& y, float& z) {
    unsigned latIdx = (index >> 8) & 0xFF;
    unsigned lngIdx = index & 0xFF;

    float lat = latIdx * (M_PI / 128.0f);
    float lng = lngIdx * (M_PI / 128.0f);

    x = cosf(lat) * sinf(lng);
    y = sinf(lat) * sinf(lng);
    z = cosf(lng);
}

PUBLIC STATIC void MD3Model::ReadShader(Stream* stream) {
    char name[MAX_QPATH + 1];
    memset(name, '\0', sizeof name);
    stream->ReadBytes(name, MAX_QPATH);

    std::string shaderName = std::string(name);

    MaterialNames.push_back(shaderName);
}

PUBLIC STATIC void MD3Model::ReadVerticesAndNormals(Vector3* vert, Vector3* norm, Sint32 vertexCount, Stream* stream) {
    for (Sint32 i = 0; i < vertexCount; i++) {
        // MD3 coordinate system is usually Z up and Y forward (I think.)
        vert->X = stream->ReadInt16() * FP16_TO(MD3_XYZ_SCALE);
        vert->Z = stream->ReadInt16() * FP16_TO(MD3_XYZ_SCALE);
        vert->Y = stream->ReadInt16() * FP16_TO(MD3_XYZ_SCALE);
        vert++;

        // Normal
        float nx, ny, nz;
        DecodeNormal(stream->ReadInt16(), nx, ny, nz);
        norm->X = nx * 32767;
        norm->Y = nz * 32767; // Not a typo; see above.
        norm->Z = ny * 32767;
        norm++;
    }
}

PUBLIC STATIC void MD3Model::ReadUVs(Vector2* uvs, Sint32 vertexCount, Stream* stream) {
    for (Sint32 i = 0; i < vertexCount; i++) {
        uvs->X = (int)(stream->ReadFloat() * 0x10000);
        uvs->Y = (int)(stream->ReadFloat() * 0x10000);
        uvs++;
    }
}

PUBLIC STATIC void MD3Model::ReadVertexIndices(Sint32* indices, Sint32 triangleCount, Stream* stream) {
    for (Sint32 i = 0; i < triangleCount; i++) {
        *indices++ = stream->ReadUInt32();
        *indices++ = stream->ReadUInt32();
        *indices++ = stream->ReadUInt32();
    }

    *indices = -1;
}

PUBLIC STATIC Mesh* MD3Model::ReadSurface(IModel* model, Stream* stream, size_t surfaceDataOffset) {
    Sint32 magic = stream->ReadUInt32BE();
    if (magic != MD3_MODEL_MAGIC) {
        Log::Print(Log::LOG_ERROR, "Invalid magic for MD3 surface!");
        return nullptr;
    }

    // Read mesh data
    Mesh* mesh = new Mesh;

    // Read name
    mesh->Name = (char *)Memory::Calloc(1, MAX_QPATH + 1);

    stream->ReadBytes(mesh->Name, MAX_QPATH);

    // Flags. Purpose seems unknown.
    stream->ReadInt32();

    Sint32 frameCount = stream->ReadInt32();
    Sint32 shaderCount = stream->ReadInt32();
    Sint32 vertexCount = stream->ReadInt32();
    Sint32 triangleCount = stream->ReadInt32();

    Sint32 triangleDataOffset = stream->ReadInt32();
    Sint32 shaderDataOffset = stream->ReadInt32();
    Sint32 stDataOffset = stream->ReadInt32();
    Sint32 vertexDataOffset = stream->ReadInt32();

    DataEndOffset = stream->ReadInt32();

    mesh->VertexFlag = VertexType_Position | VertexType_Normal | VertexType_UV;

    mesh->VertexCount = vertexCount;
    mesh->FrameCount = frameCount;

    mesh->PositionBuffer = (Vector3*)Memory::Malloc(vertexCount * frameCount * sizeof(Vector3));
    mesh->NormalBuffer = (Vector3*)Memory::Malloc(vertexCount * frameCount * sizeof(Vector3));
    mesh->UVBuffer = (Vector2*)Memory::Malloc(vertexCount * frameCount * sizeof(Vector2));

    mesh->VertexIndexCount = triangleCount * 3;
    model->VertexIndexCount += mesh->VertexIndexCount;

    mesh->VertexIndexBuffer = (Sint32*)Memory::Malloc((mesh->VertexIndexCount + 1) * sizeof(Sint32));

    const size_t surfaceDataSize = sizeof(Sint16) * 4;

    // Read vertices and normals
    Vector3* vert = mesh->PositionBuffer;
    Vector3* norm = mesh->NormalBuffer;

    for (Sint32 i = 0; i < frameCount; i++) {
        stream->Seek(surfaceDataOffset + vertexDataOffset + (i * vertexCount * surfaceDataSize));

        ReadVerticesAndNormals(vert, norm, vertexCount, stream);

        vert += vertexCount;
        norm += vertexCount;
    }

    // Read UVs
    Vector2* uv = mesh->UVBuffer;

    stream->Seek(surfaceDataOffset + stDataOffset);

    ReadUVs(uv, vertexCount, stream);

    // Copy the values to other frames
    for (Sint32 i = 0; i < vertexCount; i++) {
        Vector2* uvA = &mesh->UVBuffer[i];

        for (Sint32 j = 1; j < frameCount; j++) {
            Vector2* uvB = &mesh->UVBuffer[i + (vertexCount * j)];
            uvB->X = uvA->X;
            uvB->Y = uvA->X;
        }
    }

    // Read vertex indices
    stream->Seek(surfaceDataOffset + triangleDataOffset);

    ReadVertexIndices(mesh->VertexIndexBuffer, triangleCount, stream);

    // Read shaders
    const size_t shaderDataSize = (sizeof(Uint8) * 64) + sizeof(Sint32);

    if (shaderCount) {
        if (shaderCount > MD3_MAX_SHADERS) {
            Log::Print(Log::LOG_WARN,
                "MD3 surface has a shader count higher than the usual (maximum is %d, but surface has %d)",
                MD3_MAX_SHADERS, shaderCount);
        }

        mesh->MaterialIndex = MaterialNames.size();
    }

    for (Sint32 i = 0; i < shaderCount; i++) {
        stream->Seek(surfaceDataOffset + shaderDataOffset + (i * shaderDataSize));
        ReadShader(stream);
    }

    return mesh;
}

PUBLIC STATIC bool MD3Model::Convert(IModel* model, Stream* stream, const char* path) {
    if (stream->ReadUInt32BE() != MD3_MODEL_MAGIC) {
        Log::Print(Log::LOG_ERROR, "Model not of MD3 (Quake III) type!");
        return false;
    }

    Version = stream->ReadInt32();

    if (Version != MD3_VERSION) {
        Log::Print(Log::LOG_ERROR, "Unsupported MD3 model version!");
        return false;
    }

    char name[MAX_QPATH + 1];
    memset(name, '\0', sizeof name);
    stream->ReadBytes(name, MAX_QPATH);

    stream->ReadInt32(); // Flags. Purpose seems unknown.

    Sint32 frameCount = stream->ReadInt32();
    Sint32 tagCount = stream->ReadInt32();
    Sint32 surfCount = stream->ReadInt32();

    if (!surfCount) {
        Log::Print(Log::LOG_ERROR, "MD3 model has no surfaces!");
        return false;
    }

    if (frameCount > MD3_MAX_FRAMES) {
        Log::Print(Log::LOG_WARN,
            "MD3 model has a frame count higher than the usual (maximum is %d, but model has %d)",
            MD3_MAX_FRAMES, frameCount);
    }
    if (tagCount > MD3_MAX_TAGS) {
        Log::Print(Log::LOG_WARN,
            "MD3 model has a tag count higher than the usual (maximum is %d, but model has %d)",
            MD3_MAX_TAGS, tagCount);
    }
    if (surfCount > MD3_MAX_SURFACES) {
        Log::Print(Log::LOG_WARN,
            "MD3 model has a surface count higher than the usual (maximum is %d, but model has %d)",
            MD3_MAX_SURFACES, surfCount);
    }

    stream->ReadInt32(); // Skin count. Apparently a leftover from the previous MD2 format.

    Sint32 frameDataOffset = stream->ReadInt32();
    Sint32 tagDataOffset = stream->ReadInt32();
    Sint32 surfaceDataOffset = stream->ReadInt32();
    Sint32 dataEndOffset = stream->ReadInt32();

    // Create model
    model->Meshes = new Mesh*[surfCount];
    model->MeshCount = surfCount;
    model->VertexCount = 0;
    model->VertexPerFace = 3;
    model->UseVertexAnimation = true;

    // Read surfaces
    stream->Seek(surfaceDataOffset);

    DataEndOffset = 0;
    MaterialNames.clear();

    size_t offset = surfaceDataOffset;

    for (Sint32 i = 0; i < surfCount; i++) {
        Mesh* mesh = ReadSurface(model, stream, offset);
        if (mesh == nullptr)
            return false;

        model->Meshes[i] = mesh;
        model->VertexCount += mesh->VertexCount;

        offset += DataEndOffset;

        stream->Seek(offset);
    }

    model->FrameCount = model->Meshes[0]->FrameCount;

    // Add materials
    size_t numMaterials = MaterialNames.size();
    if (numMaterials) {
        model->MaterialCount = numMaterials;
        model->Materials = new Material*[numMaterials];

        const char *parentDirectory = StringUtils::GetPath(path);

        for (size_t i = 0; i < numMaterials; i++) {
            Material* material = new Material();
            material->ImagePtr = IModel::LoadMaterialImage(MaterialNames[i], parentDirectory);
            model->Materials[i] = material;
        }

        MaterialNames.clear();
    }

    return true;
}
