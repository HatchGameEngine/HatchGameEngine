#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Mesh.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Graphics.h>
#include <Engine/IO/Stream.h>

class IModel {
public:
    Mesh**              Meshes;
    size_t              MeshCount;

    size_t              VertexCount;
    size_t              FrameCount;
    size_t              VertexIndexCount;

    Uint8               VertexFlag;
    Uint8               FaceVertexCount;

    Material**          Materials;
    size_t              MaterialCount;

    ModelAnim**         Animations;
    size_t              AnimationCount;

    bool                UseVertexAnimation;

    ModelNode*          RootNode;
    Matrix4x4*          GlobalInverseMatrix;
};
#endif

#include <Engine/ResourceTypes/IModel.h>
#include <Engine/Math/FixedPoint.h>
#include <Engine/Math/Vector.h>

#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/ModelFormats/Importer.h>
#include <Engine/Utilities/StringUtils.h>

#define RSDK_MODEL_MAGIC 0x4D444C00 // MDL0

PUBLIC IModel::IModel() {
    MeshCount = 0;
    VertexCount = 0;
    FrameCount = 0;
    AnimationCount = 0;

    Meshes = nullptr;
    VertexIndexCount = 0;

    VertexFlag = 0;
    FaceVertexCount = 0;

    Materials = nullptr;
    Animations = nullptr;
    UseVertexAnimation = false;

    RootNode = nullptr;
    GlobalInverseMatrix = nullptr;
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

    Uint32 magic = stream->ReadUInt32BE();

    stream->Seek(0);

    if (magic == RSDK_MODEL_MAGIC)
        return this->ReadRSDK(stream);

    return !!ModelImporter::Convert(this, stream, filename);
}

PUBLIC ModelNode* IModel::SearchNode(ModelNode* node, char* name) {
    if (!strcmp(node->Name, name))
        return node;

    for (size_t i = 0; i < node->Children.size(); i++) {
        ModelNode* found = SearchNode(node->Children[i], name);
        if (found != nullptr)
            return found;
    }

    return nullptr;
}

PUBLIC void IModel::TransformNode(ModelNode* node, Matrix4x4* parentMatrix) {
    Matrix4x4::Multiply(node->GlobalTransform, node->LocalTransform, parentMatrix);

    for (size_t i = 0; i < node->Children.size(); i++)
        TransformNode(node->Children[i], node->GlobalTransform);
}

PUBLIC void IModel::AnimateNode(ModelNode* node, ModelAnim* animation, Uint32 frame, Matrix4x4* parentMatrix) {
    NodeAnim* nodeAnim = animation->NodeLookup->Get(node->Name);
    if (nodeAnim)
        UpdateChannel(node->LocalTransform, nodeAnim, frame);

    Matrix4x4::Multiply(node->GlobalTransform, node->LocalTransform, parentMatrix);

    for (size_t i = 0; i < node->Children.size(); i++)
        AnimateNode(node->Children[i], animation, frame, node->GlobalTransform);
}

PUBLIC void IModel::CalculateBones(Mesh* mesh) {
    if (!mesh->NumBones)
        return;

    for (size_t i = 0; i < mesh->NumBones; i++) {
        MeshBone* bone = mesh->Bones[i];

        Matrix4x4::Multiply(bone->FinalTransform, bone->InverseBindMatrix, bone->GlobalTransform);
        Matrix4x4::Multiply(bone->FinalTransform, GlobalInverseMatrix, bone->FinalTransform);
    }
}

static Vector3 MultiplyMatrix3x3(Vector3* v, Matrix4x4* m) {
    Vector3 result;

    Sint64 mat11 = m->Values[0]  * 0x10000;
    Sint64 mat12 = m->Values[1]  * 0x10000;
    Sint64 mat13 = m->Values[2]  * 0x10000;
    Sint64 mat21 = m->Values[4]  * 0x10000;
    Sint64 mat22 = m->Values[5]  * 0x10000;
    Sint64 mat23 = m->Values[6]  * 0x10000;
    Sint64 mat31 = m->Values[8]  * 0x10000;
    Sint64 mat32 = m->Values[9]  * 0x10000;
    Sint64 mat33 = m->Values[10] * 0x10000;

    result.X = FP16_MULTIPLY(mat11, v->X) + FP16_MULTIPLY(mat12, v->Y) + FP16_MULTIPLY(mat13, v->Z);
    result.Y = FP16_MULTIPLY(mat21, v->X) + FP16_MULTIPLY(mat22, v->Y) + FP16_MULTIPLY(mat23, v->Z);
    result.Z = FP16_MULTIPLY(mat31, v->X) + FP16_MULTIPLY(mat32, v->Y) + FP16_MULTIPLY(mat33, v->Z);

    return result;
}

PUBLIC void IModel::TransformMesh(Mesh* mesh, Vector3* outPositions, Vector3* outNormals) {
    if (!mesh->NumBones)
        return;

    memset(outPositions, 0x00, mesh->NumVertices * sizeof(Vector3));
    if (outNormals)
        memset(outNormals, 0x00, mesh->NumVertices * sizeof(Vector3));

    for (size_t i = 0; i < mesh->NumBones; i++) {
        MeshBone* bone = mesh->Bones[i];

        for (size_t w = 0; w < bone->Weights.size(); w++) {
            BoneWeight& boneWeight = bone->Weights[w];

            Uint32 vertexID = boneWeight.VertexID;
            Sint64 weight = FP16_DIVIDE(boneWeight.Weight, mesh->VertexWeights[vertexID]);

            Vector3 temp = Vector::Multiply(mesh->PositionBuffer[vertexID], bone->FinalTransform);

            outPositions[vertexID].X += FP16_MULTIPLY(temp.X, weight);
            outPositions[vertexID].Y += FP16_MULTIPLY(temp.Y, weight);
            outPositions[vertexID].Z += FP16_MULTIPLY(temp.Z, weight);

            if (!outNormals)
                continue;

            temp = MultiplyMatrix3x3(&mesh->NormalBuffer[vertexID], bone->FinalTransform);

            outNormals[vertexID].X += FP16_MULTIPLY(temp.X, weight);
            outNormals[vertexID].Y += FP16_MULTIPLY(temp.Y, weight);
            outNormals[vertexID].Z += FP16_MULTIPLY(temp.Z, weight);
        }
    }
}

PUBLIC void IModel::Pose() {
    Matrix4x4 identity;
    Matrix4x4::Identity(&identity);

    TransformNode(RootNode, &identity);
}

PUBLIC void IModel::Pose(ModelAnim* animation, Uint32 frame) {
    Matrix4x4 identity;
    Matrix4x4::Identity(&identity);

    AnimateNode(RootNode, animation, frame, &identity);
}

static void MakeChannelMatrix(Matrix4x4* out, Vector3* pos, Vector4* rot, Vector3* scale) {
    float rotX = FP16_FROM(rot->X);
    float rotY = FP16_FROM(rot->Y);
    float rotZ = FP16_FROM(rot->Z);
    float rotW = FP16_FROM(rot->W);

    float rXX = rotX * rotX;
    float rYY = rotY * rotY;
    float rZZ = rotZ * rotZ;

    float rXY = rotX * rotY;
    float rXZ = rotX * rotZ;
    float rXW = rotX * rotW;
    float rYZ = rotY * rotZ;
    float rYW = rotY * rotW;
    float rZW = rotZ * rotW;

    out->Values[0]  = 1.0f - (2.0f * (rYY + rZZ));
    out->Values[1]  = 2.0f * (rXY - rZW);
    out->Values[2]  = 2.0f * (rXZ + rYW);
    out->Values[3]  = FP16_FROM(pos->X);

    out->Values[4]  = 2.0f * (rXY + rZW);
    out->Values[5]  = 1.0f - (2.0f * (rXX + rZZ));
    out->Values[6]  = 2.0f * (rYZ - rXW);
    out->Values[7]  = FP16_FROM(pos->Y);

    out->Values[8]  = 2.0f * (rXZ - rYW);
    out->Values[9]  = 2.0f * (rYZ + rXW);
    out->Values[10] = 1.0f - (2.0f * (rXX + rYY));
    out->Values[11] = FP16_FROM(pos->Z);

    out->Values[12] = 0.0f;
    out->Values[13] = 0.0f;
    out->Values[14] = 0.0f;
    out->Values[15] = 1.0f;

    float scaleX = FP16_FROM(scale->X);
    float scaleY = FP16_FROM(scale->Y);
    float scaleZ = FP16_FROM(scale->Z);

    out->Values[0]  = out->Values[0]  * scaleX;
    out->Values[1]  = out->Values[1]  * scaleX;
    out->Values[2]  = out->Values[2]  * scaleX;

    out->Values[4]  = out->Values[4]  * scaleY;
    out->Values[5]  = out->Values[5]  * scaleY;
    out->Values[6]  = out->Values[6]  * scaleY;

    out->Values[8]  = out->Values[8]  * scaleZ;
    out->Values[9]  = out->Values[9]  * scaleZ;
    out->Values[10] = out->Values[10] * scaleZ;
}

static Vector4 InterpolateQuaternions(Vector4 q1, Vector4 q2, Sint64 t) {
    Vector4 v1 = q1;
    Vector4 v2 = q2;

    if (Vector::DotProduct(v1, v2) < 0) {
        v1.X = -v1.X;
        v1.Y = -v1.Y;
        v1.Z = -v1.Z;
        v1.W = -v1.W;
    }

    // I'm just gonna do it all directly
    Vector4 result;
    result.X = v1.X + FP16_MULTIPLY(v2.X - v1.X, t);
    result.Y = v1.Y + FP16_MULTIPLY(v2.Y - v1.Y, t);
    result.Z = v1.Z + FP16_MULTIPLY(v2.Z - v1.Z, t);
    result.W = v1.W + FP16_MULTIPLY(v2.W - v1.W, t);

    Sint64 length = Vector::Length(result);
    if (length == 0) {
        result.X = result.Y = result.Z = 0;
        result.W = 0x10000;
    }
    else {
        result.X = FP16_DIVIDE(result.X, length);
        result.Y = FP16_DIVIDE(result.Y, length);
        result.Z = FP16_DIVIDE(result.Z, length);
        result.W = FP16_DIVIDE(result.W, length);
    }

    return result;
}

PUBLIC Uint32 IModel::GetKeyFrame(Uint32 frame) {
    return (frame >> 8) & 0xFFFFFF;
}

PUBLIC Sint64 IModel::GetInBetween(Uint32 frame) {
    float interp = (float)(frame & 0xFF) / 0xFF;
    Sint64 inbetween = (Sint64)(interp * 0x10000);

    if (inbetween < 0)
        inbetween = 0;
    else if (inbetween > 0x10000)
        inbetween = 0x10000;

    return inbetween;
}

PRIVATE void IModel::UpdateChannel(Matrix4x4* out, NodeAnim* channel, Uint32 frame) {
    Uint32 keyframe = GetKeyFrame(frame);
    Sint64 inbetween = GetInBetween(frame);

    // TODO: Use the keys' time values instead of what I'm doing right now
    if (inbetween == 0) {
        AnimVectorKey& p1 = channel->PositionKeys[keyframe % channel->NumPositionKeys];
        AnimQuaternionKey& r1 = channel->RotationKeys[keyframe % channel->NumRotationKeys];
        AnimVectorKey& s1 = channel->ScalingKeys[keyframe % channel->NumScalingKeys];

        MakeChannelMatrix(out, &p1.Value, &r1.Value, &s1.Value);
    }
    else if (inbetween == 0x10000) {
        AnimVectorKey& p2 = channel->PositionKeys[(keyframe+1) % channel->NumPositionKeys];
        AnimQuaternionKey& r2 = channel->RotationKeys[(keyframe+1) % channel->NumRotationKeys];
        AnimVectorKey& s2 = channel->ScalingKeys[(keyframe+1) % channel->NumScalingKeys];

        MakeChannelMatrix(out, &p2.Value, &r2.Value, &s2.Value);
    }
    else {
        AnimVectorKey& p1 = channel->PositionKeys[keyframe % channel->NumPositionKeys];
        AnimVectorKey& p2 = channel->PositionKeys[(keyframe+1) % channel->NumPositionKeys];

        AnimQuaternionKey& r1 = channel->RotationKeys[keyframe % channel->NumRotationKeys];
        AnimQuaternionKey& r2 = channel->RotationKeys[(keyframe+1) % channel->NumRotationKeys];

        AnimVectorKey& s1 = channel->ScalingKeys[keyframe % channel->NumScalingKeys];
        AnimVectorKey& s2 = channel->ScalingKeys[(keyframe+1) % channel->NumScalingKeys];

        Vector3 pos = Vector::Interpolate(p1.Value, p2.Value, inbetween);
        Vector4 rot = InterpolateQuaternions(r1.Value, r2.Value, inbetween);
        Vector3 scale = Vector::Interpolate(s1.Value, s2.Value, inbetween);

        MakeChannelMatrix(out, &pos, &rot, &scale);
    }
}

PUBLIC void IModel::Animate(ModelAnim* animation, Uint32 frame) {
    Pose(animation, frame);

    for (size_t i = 0; i < MeshCount; i++) {
        Mesh* mesh = Meshes[i];
        if (!mesh->UseSkeleton || !mesh->TransformedPositions)
            continue;

        CalculateBones(mesh);
        TransformMesh(mesh, mesh->TransformedPositions, mesh->TransformedNormals);
    }
}

PUBLIC void IModel::Dispose() {
    VertexCount = 0;
    FrameCount = 0;
    UseVertexAnimation = false;

    VertexFlag = 0;
    VertexIndexCount = 0;
    FaceVertexCount = 0;

    for (size_t i = 0; i < MeshCount; i++)
        delete Meshes[i];

    delete[] Meshes;
    Meshes = nullptr;
    MeshCount = 0;

    for (size_t i = 0; i < MaterialCount; i++)
        delete Materials[i];
    delete[] Materials;
    Materials = nullptr;
    MaterialCount = 0;

    for (size_t i = 0; i < AnimationCount; i++)
        delete Animations[i];
    delete[] Animations;
    Animations = nullptr;
    AnimationCount = 0;

    delete RootNode;
    RootNode = nullptr;

    delete GlobalInverseMatrix;
    GlobalInverseMatrix = nullptr;
}

PUBLIC IModel::~IModel() {
    Dispose();
}

PUBLIC bool IModel::ReadRSDK(Stream* stream) {
    if (stream->ReadUInt32BE() != RSDK_MODEL_MAGIC) {
        Log::Print(Log::LOG_ERROR, "Model not of RSDK type!");
        return false;
    }

    Uint8 VertexFlag = stream->ReadByte();
    FaceVertexCount = stream->ReadByte();
    VertexCount = stream->ReadUInt16();
    FrameCount = stream->ReadUInt16();
    AnimationCount = 1;

    // We only need one mesh for RSDK models
    Mesh* mesh = new Mesh;
    Meshes = new Mesh*[1];
    Meshes[0] = mesh;
    MeshCount = 1;

    mesh->VertexFlag = VertexFlag;
    mesh->PositionBuffer = (Vector3*)Memory::Malloc(VertexCount * FrameCount * sizeof(Vector3));

    if (VertexFlag & VertexType_Normal)
        mesh->NormalBuffer = (Vector3*)Memory::Malloc(VertexCount * FrameCount * sizeof(Vector3));

    if (VertexFlag & VertexType_UV)
        mesh->UVBuffer = (Vector2*)Memory::Malloc(VertexCount * FrameCount * sizeof(Vector2));

    if (VertexFlag & VertexType_Color)
        mesh->ColorBuffer = (Uint32*)Memory::Malloc(VertexCount * FrameCount * sizeof(Uint32));

    // Read UVs
    if (VertexFlag & VertexType_UV) {
        int uvX, uvY;
        for (int i = 0; i < VertexCount; i++) {
            Vector2* uv = &mesh->UVBuffer[i];
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
            colorPtr = &mesh->ColorBuffer[i];
            *colorPtr = color = stream->ReadUInt32();
            // Copy the value to other frames
            for (int f = 1; f < FrameCount; f++) {
                colorPtr += VertexCount;
                *colorPtr = color;
            }
        }
    }

    Materials = nullptr;
    Animations = nullptr;
    RootNode = nullptr;
    GlobalInverseMatrix = nullptr;
    UseVertexAnimation = true;

    VertexIndexCount = stream->ReadInt16();
    mesh->VertexIndexCount = VertexIndexCount;
    mesh->VertexIndexBuffer = (Sint16*)Memory::Malloc((VertexIndexCount + 1) * sizeof(Sint16));

    for (int i = 0; i < VertexIndexCount; i++)
        mesh->VertexIndexBuffer[i] = stream->ReadInt16();
    mesh->VertexIndexBuffer[VertexIndexCount] = -1;

    if (VertexFlag & VertexType_Normal) {
        Vector3* vert = mesh->PositionBuffer;
        Vector3* norm = mesh->NormalBuffer;
        int totalVertexCount = VertexCount * FrameCount;
        for (int v = 0; v < totalVertexCount; v++) {
            vert->X = (int)(stream->ReadFloat() * 0x10000);
            vert->Y = (int)(stream->ReadFloat() * 0x10000);
            vert->Z = (int)(stream->ReadFloat() * 0x10000);
            vert++;

            norm->X = (int)(stream->ReadFloat() * 0x10000);
            norm->Y = (int)(stream->ReadFloat() * 0x10000);
            norm->Z = (int)(stream->ReadFloat() * 0x10000);
            norm++;
        }
    }
    else {
        Vector3* vert = mesh->PositionBuffer;
        int totalVertexCount = VertexCount * FrameCount;
        for (int v = 0; v < totalVertexCount; v++) {
            vert->X = (int)(stream->ReadFloat() * 0x10000);
            vert->Y = (int)(stream->ReadFloat() * 0x10000);
            vert->Z = (int)(stream->ReadFloat() * 0x10000);
            vert++;
        }
    }

    return true;
}
