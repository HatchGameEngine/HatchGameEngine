#include <Engine/Diagnostics/Log.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Rendering/Mesh.h>
#include <Engine/ResourceTypes/ModelFormats/RSDKModel.h>

#define RSDK_MODEL_MAGIC 0x4D444C00 // MDL0

bool RSDKModel::IsMagic(Stream* stream) {
	Uint32 magic = stream->ReadUInt32BE();

	stream->Skip(-4);

	return magic == RSDK_MODEL_MAGIC;
}

bool RSDKModel::Convert(IModel* model, Stream* stream) {
	if (stream->ReadUInt32BE() != RSDK_MODEL_MAGIC) {
		Log::Print(Log::LOG_ERROR, "Model not of RSDK type!");
		return false;
	}

	Uint8 vertexFlag = stream->ReadByte();

	model->VertexPerFace = stream->ReadByte();

	Uint16 vertexCount = stream->ReadUInt16();
	Uint16 frameCount = stream->ReadUInt16();

	// We only need one mesh for RSDK models
	Mesh* mesh = new Mesh;
	mesh->Name = StringUtils::Create("Mesh");
	model->Meshes.push_back(mesh);
	model->VertexCount = vertexCount;

	mesh->VertexCount = vertexCount;
	mesh->FrameCount = frameCount;

	mesh->VertexFlag = vertexFlag;
	mesh->PositionBuffer =
		(Vector3*)Memory::Malloc(mesh->VertexCount * frameCount * sizeof(Vector3));

	if (vertexFlag & VertexType_Normal) {
		mesh->NormalBuffer =
			(Vector3*)Memory::Malloc(mesh->VertexCount * frameCount * sizeof(Vector3));
	}

	if (vertexFlag & VertexType_UV) {
		mesh->UVBuffer =
			(Vector2*)Memory::Malloc(mesh->VertexCount * frameCount * sizeof(Vector2));
	}

	if (vertexFlag & VertexType_Color) {
		mesh->ColorBuffer =
			(Uint32*)Memory::Malloc(mesh->VertexCount * frameCount * sizeof(Uint32));
	}

	// Read UVs
	if (vertexFlag & VertexType_UV) {
		int uvX, uvY;
		for (size_t i = 0; i < mesh->VertexCount; i++) {
			Vector2* uv = &mesh->UVBuffer[i];
			uv->X = uvX = (int)(stream->ReadFloat() * 0x10000);
			uv->Y = uvY = (int)(stream->ReadFloat() * 0x10000);
			// Copy the values to other frames
			for (size_t f = 1; f < frameCount; f++) {
				uv += mesh->VertexCount;
				uv->X = uvX;
				uv->Y = uvY;
			}
		}
	}
	// Read Colors
	if (vertexFlag & VertexType_Color) {
		Uint32 *colorPtr, color;
		for (size_t i = 0; i < mesh->VertexCount; i++) {
			colorPtr = &mesh->ColorBuffer[i];
			*colorPtr = color = stream->ReadUInt32();
			// Copy the value to other frames
			for (size_t f = 1; f < frameCount; f++) {
				colorPtr += mesh->VertexCount;
				*colorPtr = color;
			}
		}
	}

	model->BaseArmature = nullptr;
	model->GlobalInverseMatrix = nullptr;
	model->UseVertexAnimation = true;

	ModelAnim* anim = new ModelAnim;
	anim->Name = StringUtils::Duplicate("Vertex Animation");
	anim->Length = frameCount;
	model->Animations.push_back(anim);

	model->VertexIndexCount = stream->ReadInt16();
	mesh->VertexIndexCount = model->VertexIndexCount;
	mesh->VertexIndexBuffer =
		(Sint32*)Memory::Malloc((model->VertexIndexCount + 1) * sizeof(Sint32));

	for (size_t i = 0; i < model->VertexIndexCount; i++) {
		mesh->VertexIndexBuffer[i] = stream->ReadInt16();
	}
	mesh->VertexIndexBuffer[model->VertexIndexCount] = -1;

	if (vertexFlag & VertexType_Normal) {
		Vector3* vert = mesh->PositionBuffer;
		Vector3* norm = mesh->NormalBuffer;
		size_t totalVertexCount = mesh->VertexCount * frameCount;
		for (size_t v = 0; v < totalVertexCount; v++) {
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
		size_t totalVertexCount = mesh->VertexCount * frameCount;
		for (size_t v = 0; v < totalVertexCount; v++) {
			vert->X = (int)(stream->ReadFloat() * 0x10000);
			vert->Y = (int)(stream->ReadFloat() * 0x10000);
			vert->Z = (int)(stream->ReadFloat() * 0x10000);
			vert++;
		}
	}

	return true;
}
