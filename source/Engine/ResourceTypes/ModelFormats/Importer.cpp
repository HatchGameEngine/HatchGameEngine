#if INTERFACE
#include <Engine/IO/Stream.h>
#include <Engine/Rendering/Material.h>
#include <Engine/ResourceTypes/IModel.h>
class ModelImporter {
public:
    static vector<int> MeshIDs;
    static char*       ParentDirectory;
};
#endif

#include <Engine/ResourceTypes/ModelFormats/Importer.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Utilities/StringUtils.h>
#include <Engine/Diagnostics/Clock.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

vector<int> ModelImporter::MeshIDs;
char*       ModelImporter::ParentDirectory;

#define LOG_FMT(s) \
    char s[1024]; \
    va_list args; \
    va_start(args, format); \
    vsnprintf(s, sizeof s, format, args); \
    va_end(args); \
    s[sizeof(s) - 1] = 0

static void LogWarn(const char* format, ...) {
    LOG_FMT(string);
    Log::Print(Log::LOG_WARN, "Model importer: %s", string);
}

static void LogError(const char* format, ...) {
    LOG_FMT(string);
    Log::Print(Log::LOG_ERROR, "Model importer: %s", string);
}

static void LogVerbose(const char* format, ...) {
    LOG_FMT(string);
    Log::Print(Log::LOG_VERBOSE, "Model importer: %s", string);
}

static void CopyColors(float dest[4], aiColor4D& src) {
    dest[0] = src.r;
    dest[1] = src.g;
    dest[2] = src.b;
    dest[3] = src.a;
}

static char* CopyString(aiString src) {
    const char* cStr = src.C_Str();
    size_t length = strlen(cStr) + 1;

    char* string = (char*)Memory::Malloc(length);
    if (!string) {
        Log::Print(Log::LOG_ERROR, "Out of memory loading model");
        exit(-1);
    }

    memcpy(string, cStr, length);
    return string;
}

static Matrix4x4* CopyMatrix(aiMatrix4x4 mat) {
    Matrix4x4* out = Matrix4x4::Create();

    for (size_t i = 0; i < 16; i++)
        out->Values[i] = mat[i >> 2][i & 3];

    return out;
}

static Image* LoadImage(const char* path) {
    Image* image = new Image(path);

    // Couldn't find an image, so try again, prefixing the parent path
    if (!image->TexturePtr && ModelImporter::ParentDirectory) {
        image->Dispose();

        char* concat = StringUtils::ConcatPaths(ModelImporter::ParentDirectory, path);
        image = new Image(concat);

        free(concat);
    }

    // Well, I tried
    if (!image->TexturePtr) {
        image->Dispose();
        image = nullptr;
    }

    return image;
}

static AnimBehavior ConvertPrePostState(aiAnimBehaviour state) {
    switch (state) {
        case aiAnimBehaviour_CONSTANT:
            return AnimBehavior_CONSTANT;
        case aiAnimBehaviour_LINEAR:
            return AnimBehavior_LINEAR;
        case aiAnimBehaviour_REPEAT:
            return AnimBehavior_REPEAT;
        default:
            return AnimBehavior_DEFAULT;
    }
}

static AnimVectorKey GetVectorKey(struct aiVectorKey* vecKey) {
    AnimVectorKey key;

    key.Time = vecKey->mTime;
    key.Value.X = (int)(vecKey->mValue.x * 0x10000);
    key.Value.Y = (int)(vecKey->mValue.y * 0x10000);
    key.Value.Z = (int)(vecKey->mValue.z * 0x10000);

    return key;
}

static AnimQuaternionKey GetQuatKey(struct aiQuatKey* quatKey) {
    AnimQuaternionKey key;

    key.Time = quatKey->mTime;
    key.Value.X = (int)(quatKey->mValue.x * 0x10000);
    key.Value.Y = (int)(quatKey->mValue.y * 0x10000);
    key.Value.Z = (int)(quatKey->mValue.z * 0x10000);
    key.Value.W = (int)(quatKey->mValue.w * 0x10000);

    return key;
}

PRIVATE STATIC void ModelImporter::LoadMesh(IModel* imodel, Mesh* mesh, struct aiMesh* amesh) {
    size_t numFaces = amesh->mNumFaces;
    size_t numVertices = amesh->mNumVertices;

    imodel->InitMesh(mesh);

    mesh->NumVertices = numVertices;

    mesh->VertexIndexCount = numFaces * 3;
    mesh->VertexIndexBuffer = (Sint16*)Memory::Malloc((mesh->VertexIndexCount + 1) * sizeof(Sint16));

    mesh->MaterialIndex = (int)amesh->mMaterialIndex;

    Vector3* vert;
    Vector3* norm = nullptr;
    Vector2* uv = nullptr;
    Uint32* color = nullptr;

    mesh->VertexFlag = VertexType_Position;
    mesh->PositionBuffer = vert = (Vector3*)Memory::Malloc(numVertices * sizeof(Vector3));

    if (amesh->HasNormals()) {
        mesh->VertexFlag |= VertexType_Normal;
        mesh->NormalBuffer = norm = (Vector3*)Memory::Malloc(numVertices * sizeof(Vector3));
    }

    if (amesh->HasTextureCoords(0)) {
        mesh->VertexFlag |= VertexType_UV;
        mesh->UVBuffer = uv = (Vector2*)Memory::Malloc(numVertices * sizeof(Vector2));
    }

    if (amesh->HasVertexColors(0)) {
        mesh->VertexFlag |= VertexType_Color;
        mesh->ColorBuffer = color = (Uint32*)Memory::Malloc(numVertices * sizeof(Uint32));
        for (int i = 0; i < numVertices; i++)
            mesh->ColorBuffer[i] = 0xFFFFFFFF;
    }

    mesh->Name = CopyString(amesh->mName);

    for (size_t v = 0; v < numVertices; v++) {
        vert->X = (int)(amesh->mVertices[v].x * 0x10000);
        vert->Y = (int)(amesh->mVertices[v].y * 0x10000);
        vert->Z = (int)(amesh->mVertices[v].z * 0x10000);

        vert++;

        if (mesh->VertexFlag & VertexType_Normal) {
            norm->X = (int)(amesh->mNormals[v].x * 0x10000);
            norm->Y = (int)(amesh->mNormals[v].y * 0x10000);
            norm->Z = (int)(amesh->mNormals[v].z * 0x10000);

            norm++;
        }

        if (mesh->VertexFlag & VertexType_UV) {
            uv->X = (int)(amesh->mTextureCoords[0][v].x * 0x10000);
            uv->Y = (int)(amesh->mTextureCoords[0][v].y * 0x10000);
            uv++;
        }

        if (mesh->VertexFlag & VertexType_Color) {
            int r = (int)(amesh->mColors[0][v].r * 0xFF);
            int g = (int)(amesh->mColors[0][v].g * 0xFF);
            int b = (int)(amesh->mColors[0][v].b * 0xFF);
            int a = (int)(amesh->mColors[0][v].a * 0xFF);
            *color = a << 24 | r << 16 | g << 8 | b;
            color++;
        }
    }

    for (size_t f = 0; f < numFaces; f++) {
        struct aiFace* face = &amesh->mFaces[f];
        mesh->VertexIndexBuffer[f * 3]     = face->mIndices[0];
        mesh->VertexIndexBuffer[f * 3 + 1] = face->mIndices[1];
        mesh->VertexIndexBuffer[f * 3 + 2] = face->mIndices[2];
    }

    mesh->VertexIndexBuffer[mesh->VertexIndexCount] = -1;
    imodel->VertexIndexCount += mesh->VertexIndexCount;
}

PRIVATE STATIC void ModelImporter::LoadMaterial(IModel* imodel, struct aiMaterial* mat) {
    Material* material = new Material();

    aiString texDiffuse;
    aiColor4D colorDiffuse;
    aiColor4D colorSpecular;
    aiColor4D colorAmbient;
    aiColor4D colorEmissive;
    ai_real shininess, shininessStrength, opacity;
    unsigned int n = 1;

    if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texDiffuse) == AI_SUCCESS)
        material->ImagePtr = LoadImage(texDiffuse.data);

    if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &colorDiffuse) == AI_SUCCESS)
        CopyColors(material->Diffuse, colorDiffuse);
    if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_SPECULAR, &colorSpecular) == AI_SUCCESS)
        CopyColors(material->Specular, colorSpecular);
    if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_AMBIENT, &colorAmbient) == AI_SUCCESS)
        CopyColors(material->Ambient, colorAmbient);
    if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_EMISSIVE, &colorEmissive) == AI_SUCCESS)
        CopyColors(material->Emissive, colorEmissive);

    if (aiGetMaterialFloatArray(mat, AI_MATKEY_SHININESS, &shininess, &n) == AI_SUCCESS)
        material->Shininess = shininess;
    if (aiGetMaterialFloatArray(mat, AI_MATKEY_SHININESS_STRENGTH, &shininessStrength, &n) == AI_SUCCESS)
        material->ShininessStrength = shininessStrength;
    if (aiGetMaterialFloatArray(mat, AI_MATKEY_OPACITY, &opacity, &n) == AI_SUCCESS)
        material->Opacity = opacity;

    imodel->Materials.push_back(material);
}

PRIVATE STATIC ModelNode* ModelImporter::LoadNode(IModel* imodel, ModelNode* parent, const struct aiNode* anode) {
    ModelNode* node = new ModelNode;

    node->Name = CopyString(anode->mName);
    node->Children.clear();
    node->Parent = parent;
    node->Transform = CopyMatrix(anode->mTransformation);
    node->LocalTransform = Matrix4x4::Create();
    node->GlobalTransform = Matrix4x4::Create();

    Matrix4x4::Copy(node->LocalTransform, node->Transform);

    for (size_t i = 0; i < anode->mNumChildren; i++)
        node->Children.push_back(LoadNode(imodel, node, anode->mChildren[i]));

    for (size_t i = 0; i < anode->mNumMeshes; i++) {
        int meshID = MeshIDs[anode->mMeshes[i]];
        if (meshID != -1)
            node->Meshes.push_back(&imodel->Meshes[meshID]);
    }

    return node;
}

PRIVATE STATIC void ModelImporter::LoadBones(IModel* imodel, Mesh* mesh, struct aiMesh* amesh) {
    mesh->UseSkeleton = true;
    mesh->Bones = new vector<MeshBone*>(amesh->mNumBones);
    mesh->VertexWeights = (Uint32*)Memory::Calloc(mesh->NumVertices, sizeof(Uint32));

    for (size_t i = 0; i < amesh->mNumBones; i++) {
        struct aiBone* abone = amesh->mBones[i];

        MeshBone* bone = new MeshBone;
        bone->Name = CopyString(abone->mName);
        bone->InverseBindMatrix = CopyMatrix(abone->mOffsetMatrix);
        bone->FinalTransform = nullptr;
        bone->Weights.clear();

        for (size_t w = 0; w < abone->mNumWeights; w++) {
            struct aiVertexWeight& aweight = abone->mWeights[w];
            Uint32 vertexID = aweight.mVertexId;
            Uint32 weight = aweight.mWeight * 0x10000;
            if (weight != 0) {
                BoneWeight boneWeight;
                boneWeight.VertexID = vertexID;
                boneWeight.Weight = weight;
                bone->Weights.push_back(boneWeight);
            }

            mesh->VertexWeights[vertexID] += weight;
        }

        // FIXME: Blender's Collada exporter prefixes the Armature name, so this won't work as-is.
        ModelNode* node = imodel->SearchNode(imodel->RootNode, bone->Name);
        if (node)
            bone->GlobalTransform = node->GlobalTransform;
        else {
            bone->GlobalTransform = Matrix4x4::Create();
            LogWarn("In mesh %s: Couldn't find node for bone %s", mesh->Name, bone->Name);
        }

        bone->FinalTransform = Matrix4x4::Create();

        (*mesh->Bones)[i] = bone;
    }
}

PRIVATE STATIC void ModelImporter::LoadAnimation(IModel* imodel, struct aiAnimation* aanim) {
    ModelAnim* anim = new ModelAnim;
    anim->Name = CopyString(aanim->mName);
    anim->Channels.resize(aanim->mNumChannels);
    anim->NodeLookup = new HashMap<NodeAnim*>(NULL, 256); // Might be enough

    // TODO: Convert these to fixed-point
    anim->Duration = aanim->mDuration;
    anim->TicksPerSecond = aanim->mTicksPerSecond;

    for (size_t i = 0; i < aanim->mNumChannels; i++) {
        struct aiNodeAnim* channel = aanim->mChannels[i];
        NodeAnim* nodeAnim = new NodeAnim;

        nodeAnim->NodeName = CopyString(channel->mNodeName);
        nodeAnim->PostState = ConvertPrePostState(channel->mPostState);
        nodeAnim->PreState = ConvertPrePostState(channel->mPreState);

        nodeAnim->PositionKeys.resize(channel->mNumPositionKeys);
        nodeAnim->NumPositionKeys = channel->mNumPositionKeys;
        for (size_t j = 0; j < nodeAnim->NumPositionKeys; j++)
            nodeAnim->PositionKeys[j] = GetVectorKey(&channel->mPositionKeys[j]);

        nodeAnim->RotationKeys.resize(channel->mNumRotationKeys);
        nodeAnim->NumRotationKeys = channel->mNumRotationKeys;
        for (size_t j = 0; j < nodeAnim->NumRotationKeys; j++)
            nodeAnim->RotationKeys[j] = GetQuatKey(&channel->mRotationKeys[j]);

        nodeAnim->ScalingKeys.resize(channel->mNumScalingKeys);
        nodeAnim->NumScalingKeys = channel->mNumScalingKeys;
        for (size_t j = 0; j < nodeAnim->NumScalingKeys; j++)
            nodeAnim->ScalingKeys[j] = GetVectorKey(&channel->mScalingKeys[j]);

        anim->NodeLookup->Put(nodeAnim->NodeName, nodeAnim);
        anim->Channels.push_back(nodeAnim);
    }

    imodel->Animations.push_back(anim);
}

PRIVATE STATIC bool ModelImporter::DoConversion(const struct aiScene* scene, IModel* imodel) {
    if (!scene->mNumMeshes)
        return false;

    size_t meshCount = 0;
    size_t totalVertices = 0;

    int vertexFlag = 0;

    vector<struct aiMesh*> ameshes;
    ameshes.clear();

    for (size_t i = 0; i < scene->mNumMeshes; i++) {
        struct aiMesh* amesh = scene->mMeshes[i];
        if (!amesh->HasPositions()) {
            MeshIDs.push_back(-1);
            continue;
        }

        if (amesh->HasNormals())
            vertexFlag |= VertexType_Normal;
        if (amesh->HasTextureCoords(0))
            vertexFlag |= VertexType_UV;
        if (amesh->HasVertexColors(0))
            vertexFlag |= VertexType_Color;

        ameshes.push_back(amesh);
        MeshIDs.push_back(meshCount++);

        totalVertices += amesh->mNumVertices;
    }

    if (!meshCount)
        return false;

    // Maximum mesh conversions: 256
    if (meshCount > 256)
        meshCount = 256;

    // Create model
    imodel->Meshes = (Mesh*)Memory::Malloc(meshCount * sizeof(Mesh));
    imodel->MeshCount = meshCount;
    imodel->VertexCount = totalVertices;
    imodel->FaceVertexCount = 3;

    // Load materials
    for (size_t i = 0; i < scene->mNumMaterials; i++)
        LoadMaterial(imodel, scene->mMaterials[i]);

    // Load meshes
    for (size_t i = 0, meshNum = 0; i < scene->mNumMeshes, meshNum < meshCount; i++) {
        struct aiMesh* amesh = scene->mMeshes[i];
        if (amesh->HasPositions())
            LoadMesh(imodel, &imodel->Meshes[meshNum++], amesh);
    }

    // Load nodes
    imodel->RootNode = LoadNode(imodel, nullptr, scene->mRootNode);
    imodel->GlobalInverseMatrix = Matrix4x4::Create();
    Matrix4x4::Invert(imodel->GlobalInverseMatrix, imodel->RootNode->Transform);

    // Load bones
    for (size_t i = 0; i < meshCount; i++) {
        Mesh* mesh = &imodel->Meshes[i];
        if (ameshes[i]->HasBones())
            LoadBones(imodel, mesh, ameshes[i]);
    }

    // Pose and transform the meshes
    imodel->Pose();

    for (size_t i = 0; i < meshCount; i++) {
        Mesh* mesh = &imodel->Meshes[i];
        if (!mesh->UseSkeleton)
            continue;

        imodel->CalculateBones(mesh);

        mesh->TransformedPositions = (Vector3*)Memory::Malloc(mesh->NumVertices * sizeof(Vector3));
        if (mesh->VertexFlag & VertexType_Normal)
            mesh->TransformedNormals = (Vector3*)Memory::Malloc(mesh->NumVertices * sizeof(Vector3));

        imodel->TransformMesh(mesh, mesh->TransformedPositions, mesh->TransformedNormals);
    }

    // Load animations
    // FIXME: Doesn't seem to be working with Collada scenes for some reason.
    if (scene->HasAnimations()) {
        for (size_t i = 0; i < scene->mNumAnimations; i++)
            LoadAnimation(imodel, scene->mAnimations[i]);

        imodel->FrameCount = imodel->Animations.size();
        imodel->Animate(imodel->Animations[0], 0);
    }

    return true;
}

PUBLIC STATIC bool ModelImporter::Convert(IModel* model, Stream* stream, const char* path) {
    size_t size = stream->Length();
    void* data = Memory::Malloc(size);
    if (data)
        stream->ReadBytes(data, size);
    else {
        Log::Print(Log::LOG_ERROR, "Out of memory importing model %s!", path);
        return false;
    }

    Assimp::Importer importer;

    int flags = aiProcessPreset_TargetRealtime_Fast;
    flags |= aiProcess_ConvertToLeftHanded;

    const struct aiScene* scene = importer.ReadFileFromMemory(data, size, flags);
    if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
        if (!scene) {
            const char* error = importer.GetErrorString();
            if (error[0])
                LogError("Couldn't import %s: %s", path, error);
            else // No error?
                LogError("Couldn't import %s", path);
        }
        else if (!scene->mRootNode)
            LogError("Couldn't import %s: No root node", path);
        else if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
            LogError("Couldn't import %s: Scene is incomplete", path);

        free(data);
        return false;
    }

    MeshIDs.clear();
    ParentDirectory = StringUtils::GetPath(path);

    bool success;

    Clock::Start();
    success = DoConversion(scene, model);
    Log::Print(Log::LOG_VERBOSE, "Model load took %.3f ms", Clock::End());

    if (!success)
        model->Dispose();

    free(data);
    free(ParentDirectory);

    return success;
}
