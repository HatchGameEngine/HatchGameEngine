#ifndef ENGINE_RESOURCETYPES_IMODEL_H
#define ENGINE_RESOURCETYPES_IMODEL_H

#include <Engine/Graphics.h>
#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Rendering/Mesh.h>

class IModel {
private:
	void UpdateChannel(Matrix4x4* out, NodeAnim* channel, Uint32 frame);

public:
	vector<Mesh*> Meshes;
	size_t VertexCount;
	size_t VertexIndexCount;
	Uint8 VertexPerFace;
	vector<Material*> Materials;
	vector<ModelAnim*> Animations;
	vector<Armature*> Armatures;
	bool UseVertexAnimation;
	Armature* BaseArmature;
	Matrix4x4* GlobalInverseMatrix;

	IModel();
	IModel(const char* filename);
	bool Load(Stream* stream, const char* filename);
	size_t FindMaterial(const char* name);
	size_t AddMaterial(Material* material);
	size_t AddUniqueMaterial(Material* material);
	bool HasMaterials();
	bool HasBones();
	void AnimateNode(ModelNode* node,
		SkeletalAnim* animation,
		Uint32 frame,
		Matrix4x4* parentMatrix);
	void Pose();
	void Pose(Armature* armature, SkeletalAnim* animation, Uint32 frame);
	Uint32 GetKeyFrame(Uint32 frame);
	Sint64 GetInBetween(Uint32 frame);
	void DoVertexFrameInterpolation(Mesh* mesh,
		ModelAnim* animation,
		Uint32 frame,
		Vector3** positionBuffer,
		Vector3** normalBuffer,
		Vector2** uvBuffer);
	void Animate(Armature* armature, ModelAnim* animation, Uint32 frame);
	void Animate(Uint16 animation, Uint32 frame);
	int GetAnimationIndex(const char* animationName);
	int NewArmature();
	void DeleteArmature(size_t index);
	void Dispose();
	~IModel();
};

#endif /* ENGINE_RESOURCETYPES_IMODEL_H */
