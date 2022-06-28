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
};

enum AnimBehavior {
    AnimBehavior_DEFAULT,
    AnimBehavior_CONSTANT,
    AnimBehavior_LINEAR,
    AnimBehavior_REPEAT
};

struct AnimVectorKey {
    double  Time;
    Vector3 Value;
};

struct AnimQuaternionKey {
    double  Time;
    Vector4 Value;
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
};

struct ModelAnim {
    char*               Name;

    vector<NodeAnim*>   Channels;
    HashMap<NodeAnim*>* NodeLookup;

    double              Duration;
    double              TicksPerSecond;
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
    vector<MeshBone*>* Bones;
    Uint32*            VertexWeights;

    Vector3*           TransformedPositions;
    Vector3*           TransformedNormals;
};

struct ModelNode {
    char*              Name;

    ModelNode*         Parent;
    vector<ModelNode*> Children;
    Matrix4x4*         Transform;
    Matrix4x4*         LocalTransform;
    Matrix4x4*         GlobalTransform;

    vector<Mesh*>      Meshes;
};

#endif /* MESH_H */
