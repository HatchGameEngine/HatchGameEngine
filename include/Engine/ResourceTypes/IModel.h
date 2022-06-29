#ifndef ENGINE_RESOURCETYPES_IMODEL_H
#define ENGINE_RESOURCETYPES_IMODEL_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class Material;
class Matrix4x4;

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Mesh.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Graphics.h>
#include <Engine/IO/Stream.h>

class IModel {
private:
    void UpdateChannel(Matrix4x4* out, NodeAnim* channel, Uint32 frame);

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
    Armature**          ArmatureList;
    size_t              ArmatureCount;
    bool                UseVertexAnimation;
    Armature*           BaseArmature;
    Matrix4x4*          GlobalInverseMatrix;

    IModel();
    IModel(const char* filename);
    bool Load(Stream* stream, const char* filename);
    void AnimateNode(ModelNode* node, ModelAnim* animation, Uint32 frame, Matrix4x4* parentMatrix);
    void Pose();
    void Pose(Armature* armature, ModelAnim* animation, Uint32 frame);
    Uint32 GetKeyFrame(Uint32 frame);
    Sint64 GetInBetween(Uint32 frame);
    void Animate(Armature* armature, ModelAnim* animation, Uint32 frame);
    int GetAnimationIndex(const char* animationName);
    int NewArmature();
    void DeleteArmature(size_t index);
    void Dispose();
    ~IModel();
    bool ReadRSDK(Stream* stream);
};

#endif /* ENGINE_RESOURCETYPES_IMODEL_H */
