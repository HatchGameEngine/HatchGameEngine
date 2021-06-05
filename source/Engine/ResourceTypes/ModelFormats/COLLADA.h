/*

Implemented by Lactozilla
https://github.com/Lactozilla

*/

#ifndef COLLADAMODEL_H
#define COLLADAMODEL_H

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/Math/Matrix4x4.h>

typedef enum {
    DAE_X_UP,
    DAE_Y_UP,
    DAE_Z_UP,
} ColladaAssetAxis;

//
// Meshes
//

struct ColladaFloatArray {
    char*         Id;
    int           Count;

    vector<float> Contents;
};

struct ColladaNameArray {
    char*         Id;
    int           Count;

    vector<char*> Contents;
};

struct ColladaAccessor {
    char*              Source;
    char*              Parameter;

    ColladaFloatArray* FloatArray;
    ColladaNameArray*  NameArray;

    int                Count;
    int                Stride;
};

struct ColladaSource {
    char*                      Id;

    vector<ColladaFloatArray*> FloatArrays;
    vector<ColladaNameArray*>  NameArrays;
    vector<ColladaAccessor*>   Accessors;
};

struct ColladaInput {
    char*                 Semantic;
    int                   Offset;

    ColladaSource*        Source;
    vector<ColladaInput*> Children;
};

//
// Materials
//

struct ColladaImage {
    char* Id;
    char* Path;
};

struct ColladaSurface {
    char*         Id;
    char*         Type;

    ColladaImage* Image;
};

struct ColladaSampler {
    char*           Id;

    ColladaSurface* Surface;
};

struct ColladaPhongComponent {
    float           Color[4];

    ColladaSampler* Sampler;
};

struct ColladaEffect {
    char*                 Id;

    ColladaPhongComponent Specular;
    ColladaPhongComponent Ambient;
    ColladaPhongComponent Emission;
    ColladaPhongComponent Diffuse;

    float                 Shininess;
    float                 Transparency;
    float                 IndexOfRefraction;
};

struct ColladaMaterial {
    char*          Id;
    char*          Name;
    char*          EffectLink;

    ColladaEffect* Effect;
};

//
// Model data
//

struct ColladaTriangles {
    int                   Count;
    char*                 Id;

    vector<ColladaInput*> Inputs;
    vector<int>           Primitives;
};

struct ColladaVertices {
    char*                 Id;

    vector<ColladaInput*> Inputs;
};

struct ColladaPrimitive {
    float Values[3];
};

struct ColladaPreProc {
    int                       NumVertices;

    vector<ColladaPrimitive>  Positions;
    vector<ColladaPrimitive>  Normals;
    vector<ColladaPrimitive>  UVs;
    vector<ColladaPrimitive>  Colors;

    vector<int>               posIndices;
    vector<int>               texIndices;
    vector<int>               nrmIndices;
    vector<int>               colIndices;
};

struct ColladaBone {
    int               Joint;

    Matrix4x4*        LocalMatrix;
    Matrix4x4*        InverseBindMatrix;

    Matrix4x4*        LocalTransform;
    Matrix4x4*        FinalTransform;

    void*             Parent;
    void*             Children;
    int               NumChildren;
};

struct ColladaJoint {
    char*        Name;
    ColladaBone* Bone;
};

struct ColladaMesh {
    ColladaVertices           Vertices;
    vector<ColladaTriangles*> Triangles;

    vector<int>               VertexCounts;

    vector<ColladaSource*>    SourceList;
    vector<ColladaJoint*>     JointList;
    vector<ColladaMaterial*>  MaterialList;

    ColladaPreProc            Processed;
};

struct ColladaGeometry {
    char*        Id;
    char*        Name;

    ColladaMesh* Mesh;
};

struct ColladaNode {
    char*                    Id;
    char*                    Sid;
    char*                    Name;

    ColladaNode*             Parent;
    vector<ColladaNode*>     Children;

    float                    Matrix[16];

    vector<ColladaMaterial*> MaterialList;
    ColladaMesh*             Mesh;
};

struct ColladaController {
    char*                  Id;
    char*                  Skin;

    vector<ColladaSource*> SourceList;

    vector<ColladaSource*> JointList;
    vector<ColladaSource*> MatrixList;
    vector<ColladaSource*> WeightList;

    ColladaNode*           RootBone;

    float                  BindShapeMatrix[16];

    ColladaSource*         InvBindMatrixSource;

    int                    NumInputs;
    int                    NumVertexWeights;
    int                    VWJointOffset;
    int                    VWWeightsOffset;

    ColladaSource*         VWJoints;
    ColladaSource*         VWWeights;

    float*                 Weights;
    float                  NumWeights;

    vector<int>            Influences;
    VertexWeightInfo*      VWInfo;
};

struct ColladaScene {
    vector<ColladaNode*> Nodes;
    vector<ColladaMesh*> Meshes;
};

struct ColladaModel {
    vector<ColladaScene*>      Scenes;
    vector<ColladaNode*>       Nodes;
    ColladaNode*               RootNode;

    vector<ColladaMaterial*>   Materials;
    vector<ColladaSurface*>    Surfaces;
    vector<ColladaSampler*>    Samplers;
    vector<ColladaEffect*>     Effects;
    vector<ColladaImage*>      Images;

    vector<ColladaController*> Controllers;

    vector<ColladaGeometry*>   Geometries;
    vector<ColladaMesh*>       Meshes;

    ColladaAssetAxis           Axis;
};

#endif
