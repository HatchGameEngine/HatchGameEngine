#ifndef MESH_H
#define MESH_H

#include <Engine/Includes/HashMap.h>
#include <Engine/Math/FixedPoint.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Math/Vector.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Utilities/StringUtils.h>

struct BoneWeight {
	Uint32 VertexID;
	Sint64 Weight;
};

struct MeshBone {
	char* Name;

	Matrix4x4* InverseBindMatrix;
	Matrix4x4* GlobalTransform;
	Matrix4x4* FinalTransform;
	vector<BoneWeight> Weights;

	MeshBone() {
		Name = nullptr;
		InverseBindMatrix = nullptr;
		GlobalTransform = nullptr;
		FinalTransform = Matrix4x4::Create();
	}

	~MeshBone() {
		Memory::Free(Name);

		delete InverseBindMatrix;
		delete FinalTransform;

		// I don't delete its GlobalTransform because it points
		// to a node's GlobalTransform

		Weights.clear();
	}

	MeshBone* Copy() {
		MeshBone* newBone = new MeshBone;

		newBone->Name = StringUtils::Duplicate(Name);
		newBone->InverseBindMatrix = Matrix4x4::Create();
		newBone->GlobalTransform = GlobalTransform;
		newBone->Weights.resize(Weights.size());

		Matrix4x4::Copy(newBone->InverseBindMatrix, InverseBindMatrix);

		for (size_t i = 0; i < Weights.size(); i++) {
			newBone->Weights[i] = Weights[i];
		}

		return newBone;
	}
};

struct AnimVectorKey {
	double Time;
	Vector3 Value;
};

struct AnimQuaternionKey {
	double Time;
	Vector4 Value;
};

enum AnimBehavior {
	AnimBehavior_DEFAULT,
	AnimBehavior_CONSTANT,
	AnimBehavior_LINEAR,
	AnimBehavior_REPEAT
};

struct NodeAnim {
	char* NodeName;

	vector<AnimVectorKey> PositionKeys;
	vector<AnimQuaternionKey> RotationKeys;
	vector<AnimVectorKey> ScalingKeys;

	size_t NumPositionKeys;
	size_t NumRotationKeys;
	size_t NumScalingKeys;

	AnimBehavior PostState;
	AnimBehavior PreState;

	NodeAnim() {
		NodeName = nullptr;

		NumPositionKeys = 0;
		NumRotationKeys = 0;
		NumScalingKeys = 0;

		PostState = AnimBehavior_DEFAULT;
		PreState = AnimBehavior_DEFAULT;
	}

	~NodeAnim() {
		Memory::Free(NodeName);

		PositionKeys.clear();
		RotationKeys.clear();
		ScalingKeys.clear();
	}
};

struct ModelAnim;

struct SkeletalAnim {
	ModelAnim* ParentAnim;

	vector<NodeAnim*> Channels;
	HashMap<NodeAnim*>* NodeLookup;

	Uint32 DurationInFrames;

	double BaseDuration;
	double TicksPerSecond;

	~SkeletalAnim() {
		for (size_t i = 0; i < Channels.size(); i++) {
			delete Channels[i];
		}

		delete NodeLookup;
	}
};

struct ModelAnim {
	char* Name = nullptr;

	Uint32 StartFrame = 0;
	Uint32 Length = 0;

	SkeletalAnim* Skeletal = nullptr;

	~ModelAnim() {
		Memory::Free(Name);

		delete Skeletal;
	}
};

struct Skeleton {
	MeshBone** Bones;
	size_t NumBones;
	size_t NumVertices;
	Uint32* VertexWeights;

	Vector3* PositionBuffer;
	Vector3* NormalBuffer;

	bool UseTransforms;
	Vector3* TransformedPositions;
	Vector3* TransformedNormals;

	Matrix4x4* GlobalInverseMatrix; // Pointer to the model's
		// GlobalInverseMatrix

	Skeleton() {
		Bones = nullptr;
		NumBones = 0;
		NumVertices = 0;
		VertexWeights = nullptr;

		PositionBuffer = nullptr;
		NormalBuffer = nullptr;

		UseTransforms = false;
		TransformedPositions = nullptr;
		TransformedNormals = nullptr;

		GlobalInverseMatrix = nullptr;
	}

	void CalculateBones() {
		for (size_t i = 0; i < NumBones; i++) {
			MeshBone* bone = Bones[i];

			if (bone->GlobalTransform) {
				Matrix4x4::Multiply(bone->FinalTransform,
					bone->InverseBindMatrix,
					bone->GlobalTransform);
				Matrix4x4::Multiply(bone->FinalTransform,
					GlobalInverseMatrix,
					bone->FinalTransform);
			}
			else {
				// We don't have a GlobalTransform...
				// so we just use its InverseBindMatrix
				Matrix4x4::Multiply(bone->FinalTransform,
					GlobalInverseMatrix,
					bone->InverseBindMatrix);
			}
		}
	}

	static Vector3 MultiplyMatrix3x3(Vector3* v, Matrix4x4* m) {
		Vector3 result;

		Sint64 mat11 = m->Values[0] * 0x10000;
		Sint64 mat12 = m->Values[1] * 0x10000;
		Sint64 mat13 = m->Values[2] * 0x10000;
		Sint64 mat21 = m->Values[4] * 0x10000;
		Sint64 mat22 = m->Values[5] * 0x10000;
		Sint64 mat23 = m->Values[6] * 0x10000;
		Sint64 mat31 = m->Values[8] * 0x10000;
		Sint64 mat32 = m->Values[9] * 0x10000;
		Sint64 mat33 = m->Values[10] * 0x10000;

		result.X = FP16_MULTIPLY(mat11, v->X) + FP16_MULTIPLY(mat12, v->Y) +
			FP16_MULTIPLY(mat13, v->Z);
		result.Y = FP16_MULTIPLY(mat21, v->X) + FP16_MULTIPLY(mat22, v->Y) +
			FP16_MULTIPLY(mat23, v->Z);
		result.Z = FP16_MULTIPLY(mat31, v->X) + FP16_MULTIPLY(mat32, v->Y) +
			FP16_MULTIPLY(mat33, v->Z);

		return result;
	}

	void Transform() {
		Vector3* outPositions = TransformedPositions;
		Vector3* outNormals = TransformedNormals;

		memset(outPositions, 0x00, NumVertices * sizeof(Vector3));
		if (outNormals) {
			memset(outNormals, 0x00, NumVertices * sizeof(Vector3));
		}

		for (size_t i = 0; i < NumBones; i++) {
			MeshBone* bone = Bones[i];

			for (size_t w = 0; w < bone->Weights.size(); w++) {
				BoneWeight& boneWeight = bone->Weights[w];

				Uint32 vertexID = boneWeight.VertexID;
				Sint64 weight =
					FP16_DIVIDE(boneWeight.Weight, VertexWeights[vertexID]);

				Vector3 temp = Vector::Multiply(
					PositionBuffer[vertexID], bone->FinalTransform);

				outPositions[vertexID].X += FP16_MULTIPLY(temp.X, weight);
				outPositions[vertexID].Y += FP16_MULTIPLY(temp.Y, weight);
				outPositions[vertexID].Z += FP16_MULTIPLY(temp.Z, weight);

				if (!outNormals) {
					continue;
				}

				temp = Skeleton::MultiplyMatrix3x3(
					&NormalBuffer[vertexID], bone->FinalTransform);

				outNormals[vertexID].X += FP16_MULTIPLY(temp.X, weight);
				outNormals[vertexID].Y += FP16_MULTIPLY(temp.Y, weight);
				outNormals[vertexID].Z += FP16_MULTIPLY(temp.Z, weight);
			}
		}
	}

	void PrepareTransform() {
		UseTransforms = true;

		if (TransformedPositions == nullptr) {
			TransformedPositions =
				(Vector3*)Memory::Malloc(NumVertices * sizeof(Vector3));
		}

		// Only if we have a normal buffer
		if (TransformedNormals == nullptr && NormalBuffer) {
			TransformedNormals =
				(Vector3*)Memory::Malloc(NumVertices * sizeof(Vector3));
		}
	}

	~Skeleton() {
		// PositionBuffer and NormalBuffer are not owned by us,
		// so we don't delete them. GlobalInverseMatrix isn't
		// either.
		if (UseTransforms) {
			delete TransformedPositions;
			delete TransformedNormals;
		}

		Memory::Free(VertexWeights);

		for (size_t i = 0; i < NumBones; i++) {
			delete Bones[i];
		}

		delete[] Bones;
	}

	Skeleton* Copy() {
		Skeleton* newSkeleton = new Skeleton;

		newSkeleton->NumBones = NumBones;
		newSkeleton->Bones = new MeshBone*[NumBones];
		for (size_t i = 0; i < NumBones; i++) {
			newSkeleton->Bones[i] = Bones[i]->Copy();
		}

		newSkeleton->NumVertices = NumVertices;
		newSkeleton->VertexWeights = (Uint32*)Memory::Calloc(NumVertices, sizeof(Uint32));

		memcpy(newSkeleton->VertexWeights, VertexWeights, NumVertices * sizeof(Uint32));

		newSkeleton->PositionBuffer = PositionBuffer;
		newSkeleton->NormalBuffer = NormalBuffer;

		if (UseTransforms) {
			newSkeleton->PrepareTransform();
		}

		newSkeleton->GlobalInverseMatrix = GlobalInverseMatrix;

		return newSkeleton;
	}
};

struct Mesh {
	char* Name;

	Vector3* PositionBuffer;
	Vector3* NormalBuffer;
	Vector2* UVBuffer;
	Uint32* ColorBuffer;

	Uint32 VertexCount;

	// For vertex animation
	Uint32 FrameCount;
	Vector3* InbetweenPositions;
	Vector3* InbetweenNormals;

	Sint32* VertexIndexBuffer;
	Uint32 VertexIndexCount;
	Uint8 VertexFlag;

	int MaterialIndex;
	int SkeletonIndex;

	Mesh() {
		VertexCount = 0;
		FrameCount = 0;
		VertexFlag = 0;
		MaterialIndex = -1;
		SkeletonIndex = -1;
		PositionBuffer = nullptr;
		NormalBuffer = nullptr;
		UVBuffer = nullptr;
		ColorBuffer = nullptr;
		InbetweenPositions = nullptr;
		InbetweenNormals = nullptr;
		Name = nullptr;
	};

	~Mesh() {
		Memory::Free(Name);
		Memory::Free(PositionBuffer);
		Memory::Free(NormalBuffer);
		Memory::Free(UVBuffer);
		Memory::Free(ColorBuffer);
		Memory::Free(VertexIndexBuffer);
		Memory::Free(InbetweenPositions);
		Memory::Free(InbetweenNormals);
	}
};

struct ModelNode {
	char* Name;

	ModelNode* Parent;
	vector<ModelNode*> Children;

	Matrix4x4* TransformMatrix;
	Matrix4x4* LocalTransform;
	Matrix4x4* GlobalTransform;

	vector<Mesh*> Meshes;

	ModelNode() {
		Name = nullptr;
		Parent = nullptr;
		TransformMatrix = nullptr;
		LocalTransform = nullptr;
		GlobalTransform = nullptr;
	}

	~ModelNode() {
		Memory::Free(Name);

		delete TransformMatrix;
		delete LocalTransform;
		delete GlobalTransform;

		for (size_t i = 0; i < Children.size(); i++) {
			delete Children[i];
		}
	}

	ModelNode* Search(char* nodeName) {
		if (!strcmp(Name, nodeName)) {
			return this;
		}

		for (size_t i = 0; i < Children.size(); i++) {
			ModelNode* found = Children[i]->Search(nodeName);
			if (found != nullptr) {
				return found;
			}
		}

		return nullptr;
	}

	void Transform(Matrix4x4* parentMatrix) {
		Matrix4x4::Multiply(GlobalTransform, LocalTransform, parentMatrix);

		for (size_t i = 0; i < Children.size(); i++) {
			Children[i]->Transform(GlobalTransform);
		}
	}

	void Transform() {
		Matrix4x4 identity;
		Matrix4x4::Identity(&identity);
		Transform(&identity);
	}

	void Reset(Matrix4x4* parentMatrix) {
		Matrix4x4::Copy(LocalTransform, TransformMatrix);
		Matrix4x4::Multiply(GlobalTransform, LocalTransform, parentMatrix);

		for (size_t i = 0; i < Children.size(); i++) {
			Children[i]->Reset(GlobalTransform);
		}
	}

	void Reset() {
		Matrix4x4 identity;
		Matrix4x4::Identity(&identity);
		Reset(&identity);
	}

	ModelNode* Copy() {
		ModelNode* newNode = new ModelNode;

		newNode->Name = StringUtils::Duplicate(Name);
		newNode->Parent = Parent;

		newNode->Children.resize(Children.size());
		for (size_t i = 0; i < Children.size(); i++) {
			newNode->Children[i] = Children[i]->Copy();
		}

		newNode->TransformMatrix = Matrix4x4::Create();
		newNode->LocalTransform = Matrix4x4::Create();
		newNode->GlobalTransform = Matrix4x4::Create();

		Matrix4x4::Copy(newNode->TransformMatrix, TransformMatrix);
		Matrix4x4::Copy(newNode->LocalTransform, TransformMatrix);

		return newNode;
	}
};

// An armature is a collection of skeletons (which are collections of
// bones), plus a node tree (that the bones of the skeletons use for
// transforming.) In Blender, "armature" and "skeleton" mean the same
// thing.
struct Armature {
	ModelNode* RootNode;
	Skeleton** Skeletons;
	size_t NumSkeletons;

	Armature() {
		RootNode = nullptr;
		Skeletons = nullptr;
		NumSkeletons = 0;
	}

	~Armature() {
		delete RootNode;
		delete[] Skeletons;
	}

	void Reset() {
		RootNode->Reset();
		UpdateSkeletons();
	}

	void UpdateSkeletons() {
		for (size_t i = 0; i < NumSkeletons; i++) {
			Skeleton* skeleton = Skeletons[i];
			skeleton->CalculateBones();
			skeleton->Transform();
		}
	}

	Armature* Copy() {
		Armature* newArmature = new Armature;

		newArmature->RootNode = RootNode->Copy();
		newArmature->NumSkeletons = NumSkeletons;
		newArmature->Skeletons = new Skeleton*[newArmature->NumSkeletons];

		for (size_t i = 0; i < newArmature->NumSkeletons; i++) {
			Skeleton* newSkeleton = Skeletons[i]->Copy();

			// We need to point our new bones' global
			// transforms to our new nodes' global
			// transforms.
			for (size_t i = 0; i < newSkeleton->NumBones; i++) {
				MeshBone* bone = newSkeleton->Bones[i];
				ModelNode* node = newArmature->RootNode->Search(bone->Name);
				if (node) {
					bone->GlobalTransform = node->GlobalTransform;
				}
			}

			newArmature->Skeletons[i] = newSkeleton;
		}

		return newArmature;
	}
};

#endif /* MESH_H */
