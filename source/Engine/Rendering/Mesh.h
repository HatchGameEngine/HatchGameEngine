#ifndef MESH_H
#define MESH_H

#include <Engine/Rendering/3D.h>
#include <Engine/Includes/HashMap.h>

struct BoneWeight {
    Uint32 VertexID;
    Sint64 Weight;
};

struct MeshBone {
    char*                 Name;

    Matrix4x4*            InverseBindMatrix;
    Matrix4x4*            GlobalTransform;
    Matrix4x4*            FinalTransform;
    vector<BoneWeight>    Weights;

    ~MeshBone() {
        Memory::Free(Name);

        delete InverseBindMatrix;
        delete FinalTransform;

        // I don't delete its GlobalTransform because it points to a node's GlobalTransform

        Weights.clear();
    }
};

struct AnimVectorKey {
    double  Time;
    Vector3 Value;
};

struct AnimQuaternionKey {
    double  Time;
    Vector4 Value;
};

enum AnimBehavior {
    AnimBehavior_DEFAULT,
    AnimBehavior_CONSTANT,
    AnimBehavior_LINEAR,
    AnimBehavior_REPEAT
};

struct NodeAnim {
    char*                     NodeName;

    vector<AnimVectorKey>     PositionKeys;
    vector<AnimQuaternionKey> RotationKeys;
    vector<AnimVectorKey>     ScalingKeys;

    size_t                    NumPositionKeys;
    size_t                    NumRotationKeys;
    size_t                    NumScalingKeys;

    AnimBehavior              PostState;
    AnimBehavior              PreState;

    ~NodeAnim() {
        Memory::Free(NodeName);

        PositionKeys.clear();
        RotationKeys.clear();
        ScalingKeys.clear();
    }
};

struct ModelAnim {
    char*               Name;

    vector<NodeAnim*>   Channels;
    HashMap<NodeAnim*>* NodeLookup;

    Uint32              Duration;
    Uint32              TicksPerSecond;

    ~ModelAnim() {
        Memory::Free(Name);

        for (size_t i = 0; i < Channels.size(); i++)
            delete Channels[i];

        delete NodeLookup;
    }
};

struct Mesh {
    char*              Name;

    Vector3*           PositionBuffer;
    Vector3*           NormalBuffer;
    Vector2*           UVBuffer;
    Uint32*            ColorBuffer;

    size_t             NumVertices;

    Sint16*            VertexIndexBuffer;
    Uint16             VertexIndexCount;
    Uint8              VertexFlag;

    int                MaterialIndex;
    bool               UseSkeleton;

    MeshBone**         Bones;
    size_t             NumBones;

    Uint32*            VertexWeights;

    Vector3*           TransformedPositions;
    Vector3*           TransformedNormals;

    Mesh() {
        NumVertices = 0;
        VertexFlag = 0;
        MaterialIndex = -1;
        UseSkeleton = false;
        PositionBuffer = nullptr;
        NormalBuffer = nullptr;
        UVBuffer = nullptr;
        ColorBuffer = nullptr;
        TransformedPositions = nullptr;
        TransformedNormals = nullptr;
        Bones = nullptr;
        VertexWeights = nullptr;
        Name = nullptr;
    };

    ~Mesh() {
        Memory::Free(Name);
        Memory::Free(PositionBuffer);
        Memory::Free(NormalBuffer);
        Memory::Free(UVBuffer);
        Memory::Free(ColorBuffer);
        Memory::Free(VertexIndexBuffer);

        Memory::Free(VertexWeights);

        Memory::Free(TransformedPositions);
        Memory::Free(TransformedNormals);

        if (Bones) {
            for (size_t i = 0; i < NumBones; i++)
                delete Bones[i];
            delete[] Bones;
        }
    }
};

struct ModelNode {
    char*              Name;

    ModelNode*         Parent;
    vector<ModelNode*> Children;

    Matrix4x4*         Transform;
    Matrix4x4*         LocalTransform;
    Matrix4x4*         GlobalTransform;

    vector<Mesh*>      Meshes;

    ~ModelNode() {
        Memory::Free(Name);

        delete Transform;
        delete LocalTransform;
        delete GlobalTransform;

        for (size_t i = 0; i < Children.size(); i++)
            delete Children[i];
    }
};

#endif /* MESH_H */
