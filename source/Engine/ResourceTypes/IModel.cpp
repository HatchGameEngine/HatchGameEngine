#include <Engine/Math/FixedPoint.h>
#include <Engine/Math/Vector.h>
#include <Engine/ResourceTypes/IModel.h>

#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/ModelFormats/HatchModel.h>
#include <Engine/ResourceTypes/ModelFormats/Importer.h>
#include <Engine/ResourceTypes/ModelFormats/MD3Model.h>
#include <Engine/ResourceTypes/ModelFormats/RSDKModel.h>
#include <Engine/Utilities/StringUtils.h>

IModel::IModel() {
	VertexCount = 0;
	VertexIndexCount = 0;
	VertexPerFace = 0;

	Meshes.clear();
	Materials.clear();
	Animations.clear();
	Armatures.clear();

	BaseArmature = nullptr;
	GlobalInverseMatrix = nullptr;
	UseVertexAnimation = false;
}
IModel::IModel(const char* filename) {
	ResourceStream* resourceStream = ResourceStream::New(filename);
	if (!resourceStream) {
		return;
	}

	this->Load(resourceStream, filename);

	if (resourceStream) {
		resourceStream->Close();
	}
}
bool IModel::Load(Stream* stream, const char* filename) {
	if (!stream) {
		return false;
	}
	if (!filename) {
		return false;
	}

	bool success;

	Clock::Start();

	bool isHatchModel = HatchModel::IsMagic(stream);
	if (isHatchModel) {
		success = HatchModel::Convert(this, stream, filename);
	}
	else if (MD3Model::IsMagic(stream)) {
		success = MD3Model::Convert(this, stream, filename);
	}
	else if (RSDKModel::IsMagic(stream)) {
		success = RSDKModel::Convert(this, stream);
	}
	else {
		success = ModelImporter::Convert(this, stream, filename);
	}

	if (!success) {
		Log::Print(Log::LOG_ERROR, "Could not load model \"%s\"!", filename);
		return false;
	}

	Log::Print(Log::LOG_VERBOSE, "Model load took %.3f ms (%s)", Clock::End(), filename);

	if (Application::DevConvertModels && !isHatchModel) {
		Log::Print(Log::LOG_VERBOSE, "Converting model \"%s\" to HatchModel...", filename);

		std::string newFilename = std::string(filename);
		size_t pos = newFilename.find_last_of('.');
		if (pos != std::string::npos) {
			newFilename = newFilename.substr(0, pos);
		}
		newFilename = "./Resources/" + newFilename + ".hmdl";

		filename = newFilename.c_str();
		success = HatchModel::Save(this, filename);
		if (success) {
			Log::Print(Log::LOG_VERBOSE, "Saved HatchModel to \"%s\"", filename);
		}
		else {
			Log::Print(Log::LOG_ERROR, "Failed to convert HatchModel!");
		}
	}

	return true;
}

size_t IModel::FindMaterial(const char* name) {
	if (name == nullptr) {
		return -1;
	}

	for (size_t i = 0; i < Materials.size(); i++) {
		if (strcmp(Materials[i]->Name, name) == 0) {
			return i;
		}
	}

	return -1;
}

size_t IModel::AddMaterial(Material* material) {
	size_t existing = FindMaterial(material->Name);
	if (existing != -1) {
		Materials[existing] = material;
		return existing;
	}

	Materials.push_back(material);

	return Materials.size() - 1;
}

size_t IModel::AddUniqueMaterial(Material* material) {
	unsigned attempts = 0;

	while (FindMaterial(material->Name) != -1) {
		std::string matName = std::string(material->Name) + "_copy";
		if (attempts > 1) {
			matName += std::to_string(attempts);
		}
		Memory::Free(material->Name);
		material->Name = StringUtils::Create(matName);
	}

	Materials.push_back(material);

	return Materials.size() - 1;
}

bool IModel::HasMaterials() {
	return Materials.size() > 0;
}

bool IModel::HasBones() {
	if (BaseArmature == nullptr) {
		return false;
	}

	return BaseArmature->NumSkeletons > 0;
}

void IModel::AnimateNode(ModelNode* node,
	SkeletalAnim* animation,
	Uint32 frame,
	Matrix4x4* parentMatrix) {
	NodeAnim* nodeAnim = animation->NodeLookup->Get(node->Name);
	if (nodeAnim) {
		UpdateChannel(node->LocalTransform, nodeAnim, frame);
	}

	Matrix4x4::Multiply(node->GlobalTransform, node->LocalTransform, parentMatrix);

	for (size_t i = 0; i < node->Children.size(); i++) {
		AnimateNode(node->Children[i], animation, frame, node->GlobalTransform);
	}
}

void IModel::Pose() {
	BaseArmature->RootNode->Transform();
}

void IModel::Pose(Armature* armature, SkeletalAnim* animation, Uint32 frame) {
	Matrix4x4 identity;
	Matrix4x4::Identity(&identity);

	AnimateNode(armature->RootNode, animation, frame, &identity);
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

	out->Values[0] = 1.0f - (2.0f * (rYY + rZZ));
	out->Values[1] = 2.0f * (rXY - rZW);
	out->Values[2] = 2.0f * (rXZ + rYW);
	out->Values[3] = FP16_FROM(pos->X);

	out->Values[4] = 2.0f * (rXY + rZW);
	out->Values[5] = 1.0f - (2.0f * (rXX + rZZ));
	out->Values[6] = 2.0f * (rYZ - rXW);
	out->Values[7] = FP16_FROM(pos->Y);

	out->Values[8] = 2.0f * (rXZ - rYW);
	out->Values[9] = 2.0f * (rYZ + rXW);
	out->Values[10] = 1.0f - (2.0f * (rXX + rYY));
	out->Values[11] = FP16_FROM(pos->Z);

	out->Values[12] = 0.0f;
	out->Values[13] = 0.0f;
	out->Values[14] = 0.0f;
	out->Values[15] = 1.0f;

	float scaleX = FP16_FROM(scale->X);
	float scaleY = FP16_FROM(scale->Y);
	float scaleZ = FP16_FROM(scale->Z);

	out->Values[0] = out->Values[0] * scaleX;
	out->Values[1] = out->Values[1] * scaleX;
	out->Values[2] = out->Values[2] * scaleX;

	out->Values[4] = out->Values[4] * scaleY;
	out->Values[5] = out->Values[5] * scaleY;
	out->Values[6] = out->Values[6] * scaleY;

	out->Values[8] = out->Values[8] * scaleZ;
	out->Values[9] = out->Values[9] * scaleZ;
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

Uint32 IModel::GetKeyFrame(Uint32 frame) {
	return (frame >> 8) & 0xFFFFFF;
}

Sint64 IModel::GetInBetween(Uint32 frame) {
	float interp = (float)(frame & 0xFF) / 0xFF;
	Sint64 inbetween = (Sint64)(interp * 0x10000);

	if (inbetween < 0) {
		inbetween = 0;
	}
	else if (inbetween > 0x10000) {
		inbetween = 0x10000;
	}

	return inbetween;
}

void IModel::DoVertexFrameInterpolation(Mesh* mesh,
	ModelAnim* animation,
	Uint32 frame,
	Vector3** positionBuffer,
	Vector3** normalBuffer,
	Vector2** uvBuffer) {
	Uint32 startFrame = 0;
	Uint32 animLength;

	if (animation) {
		animLength = animation->Length % (mesh->FrameCount + 1);
		if (!animLength) {
			animLength = 1;
		}
		startFrame = animation->StartFrame % animLength;
	}
	else if (mesh->FrameCount) {
		animLength = mesh->FrameCount;
	}
	else {
		animLength = 1;
	}

	Uint32 keyframe = startFrame + (GetKeyFrame(frame) % animLength);
	Uint32 nextKeyframe = startFrame + ((keyframe + 1) % animLength);
	Sint64 inbetween = GetInBetween(frame);

	if (inbetween == 0 || inbetween == 0x10000) {
		if (inbetween == 0x10000) {
			keyframe = nextKeyframe;
		}

		*positionBuffer = mesh->PositionBuffer + (keyframe * mesh->VertexCount);
		*normalBuffer = mesh->NormalBuffer + (keyframe * mesh->VertexCount);
		*uvBuffer = mesh->UVBuffer + (keyframe * mesh->VertexCount);
	}
	else {
		if (mesh->InbetweenPositions == nullptr) {
			mesh->InbetweenPositions =
				(Vector3*)Memory::Malloc(mesh->VertexCount * sizeof(Vector3));
		}
		if (mesh->InbetweenNormals == nullptr) {
			mesh->InbetweenNormals =
				(Vector3*)Memory::Malloc(mesh->VertexCount * sizeof(Vector3));
		}

		Vector3* outPos = mesh->InbetweenPositions;
		Vector3* outNormal = mesh->InbetweenNormals;

		*positionBuffer = outPos;
		*normalBuffer = outNormal;

		// UVs are not interpolated
		*uvBuffer = mesh->UVBuffer + (keyframe * mesh->VertexCount);

		for (size_t i = 0; i < mesh->VertexCount; i++) {
			Vector3 posA = mesh->PositionBuffer[(keyframe * mesh->VertexCount) + i];
			Vector3 posB = mesh->PositionBuffer[(nextKeyframe * mesh->VertexCount) + i];

			Vector3 normA = mesh->NormalBuffer[(keyframe * mesh->VertexCount) + i];
			Vector3 normB = mesh->NormalBuffer[(nextKeyframe * mesh->VertexCount) + i];

			*outPos++ = Vector::Interpolate(posA, posB, inbetween);
			*outNormal++ = Vector::Interpolate(normA, normB, inbetween);
		}
	}
}

void IModel::UpdateChannel(Matrix4x4* out, NodeAnim* channel, Uint32 frame) {
	Uint32 keyframe = GetKeyFrame(frame);
	Sint64 inbetween = GetInBetween(frame);

	// TODO: Use the keys' time values instead of what I'm doing
	// right now
	if (inbetween == 0) {
		AnimVectorKey& p1 = channel->PositionKeys[keyframe % channel->NumPositionKeys];
		AnimQuaternionKey& r1 = channel->RotationKeys[keyframe % channel->NumRotationKeys];
		AnimVectorKey& s1 = channel->ScalingKeys[keyframe % channel->NumScalingKeys];

		MakeChannelMatrix(out, &p1.Value, &r1.Value, &s1.Value);
	}
	else if (inbetween == 0x10000) {
		AnimVectorKey& p2 =
			channel->PositionKeys[(keyframe + 1) % channel->NumPositionKeys];
		AnimQuaternionKey& r2 =
			channel->RotationKeys[(keyframe + 1) % channel->NumRotationKeys];
		AnimVectorKey& s2 = channel->ScalingKeys[(keyframe + 1) % channel->NumScalingKeys];

		MakeChannelMatrix(out, &p2.Value, &r2.Value, &s2.Value);
	}
	else {
		AnimVectorKey& p1 = channel->PositionKeys[keyframe % channel->NumPositionKeys];
		AnimVectorKey& p2 =
			channel->PositionKeys[(keyframe + 1) % channel->NumPositionKeys];

		AnimQuaternionKey& r1 = channel->RotationKeys[keyframe % channel->NumRotationKeys];
		AnimQuaternionKey& r2 =
			channel->RotationKeys[(keyframe + 1) % channel->NumRotationKeys];

		AnimVectorKey& s1 = channel->ScalingKeys[keyframe % channel->NumScalingKeys];
		AnimVectorKey& s2 = channel->ScalingKeys[(keyframe + 1) % channel->NumScalingKeys];

		Vector3 pos = Vector::Interpolate(p1.Value, p2.Value, inbetween);
		Vector4 rot = InterpolateQuaternions(r1.Value, r2.Value, inbetween);
		Vector3 scale = Vector::Interpolate(s1.Value, s2.Value, inbetween);

		MakeChannelMatrix(out, &pos, &rot, &scale);
	}
}

void IModel::Animate(Armature* armature, ModelAnim* animation, Uint32 frame) {
	if (animation->Skeletal == nullptr) {
		return;
	}

	if (armature == nullptr) {
		armature = BaseArmature;
	}

	Pose(armature, animation->Skeletal, frame);

	armature->UpdateSkeletons();
}

void IModel::Animate(Uint16 animation, Uint32 frame) {
	if (Animations.size() > 0) {
		if (animation >= Animations.size()) {
			animation = Animations.size() - 1;
		}

		Animate(nullptr, Animations[animation], frame);
	}
}

int IModel::GetAnimationIndex(const char* animationName) {
	if (!Animations.size()) {
		return -1;
	}

	for (size_t i = 0; i < Animations.size(); i++) {
		if (!strcmp(Animations[i]->Name, animationName)) {
			return (int)i;
		}
	}

	return -1;
}

int IModel::NewArmature() {
	if (UseVertexAnimation) {
		return -1;
	}

	Armature* armature = BaseArmature->Copy();

	// Find an unoccupied index
	for (size_t i = 0; i < Armatures.size(); i++) {
		if (Armatures[i] == nullptr) {
			Armatures[i] = armature;
			return i;
		}
	}

	Armatures.push_back(armature);

	// Pose it
	armature->RootNode->Transform();
	armature->UpdateSkeletons();

	return Armatures.size() - 1;
}

void IModel::DeleteArmature(size_t index) {
	if (index < Armatures.size()) {
		delete Armatures[index];
		Armatures[index] = nullptr;
	}
}

void IModel::Dispose() {
	// Models don't own their materials, so they do not delete
	// them.
	for (size_t i = 0; i < Meshes.size(); i++) {
		delete Meshes[i];
	}
	for (size_t i = 0; i < Animations.size(); i++) {
		delete Animations[i];
	}
	for (size_t i = 0; i < Armatures.size(); i++) {
		delete Armatures[i];
	}

	delete BaseArmature;
	delete GlobalInverseMatrix;

	Meshes.clear();
	Materials.clear();
	Animations.clear();
	Armatures.clear();

	BaseArmature = nullptr;
	GlobalInverseMatrix = nullptr;
}

IModel::~IModel() {
	Dispose();
}
