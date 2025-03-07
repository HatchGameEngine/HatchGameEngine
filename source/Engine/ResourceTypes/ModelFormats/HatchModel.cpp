#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/FileStream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Rendering/Mesh.h>
#include <Engine/ResourceTypes/ModelFormats/HatchModel.h>
#include <Engine/Utilities/ColorUtils.h>

#define HATCH_MODEL_MAGIC 0x484D444C // HMDL

Sint32 HatchModel::Version;

Uint32 HatchModel::NumVertexStore;
Uint32 HatchModel::NumNormalStore;
Uint32 HatchModel::NumTexCoordStore;
Uint32 HatchModel::NumColorStore;

Vector3* HatchModel::VertexStore;
Vector3* HatchModel::NormalStore;
Vector2* HatchModel::TexCoordStore;
Uint32* HatchModel::ColorStore;

bool HatchModel::IsMagic(Stream* stream) {
	Uint32 magic = stream->ReadUInt32BE();

	stream->Skip(-4);

	return magic == HATCH_MODEL_MAGIC;
}

void HatchModel::ReadMaterialInfo(Stream* stream, Uint8* destColors, char** texName) {
	*texName = stream->ReadString();

	Uint32 colorIndex = stream->ReadUInt32();

	Uint32 color = GetStoredColor(colorIndex);

	ColorUtils::Separate(color, destColors);
}

Material* HatchModel::ReadMaterial(Stream* stream, const char* parentDirectory) {
	char* name = stream->ReadString();

	Material* material = Material::Create(name);

	Uint8 flags = stream->ReadByte();

	// Read diffuse
	if (flags & 1) {
		char* diffuseTexture;
		Uint8 diffuseColor[4];

		ReadMaterialInfo(stream, diffuseColor, &diffuseTexture);

		ColorUtils::Separate(material->ColorDiffuse, diffuseColor);

		material->TextureDiffuse = Material::LoadForModel(diffuseTexture, parentDirectory);
		if (material->TextureDiffuse) {
			material->TextureDiffuseName =
				StringUtils::Duplicate(material->TextureDiffuse->Filename);
		}

		Memory::Free(diffuseTexture);
	}

	// Read specular
	if (flags & 2) {
		char* specularTexture;
		Uint8 specularColor[4];

		ReadMaterialInfo(stream, specularColor, &specularTexture);

		ColorUtils::Separate(material->ColorSpecular, specularColor);

		material->TextureSpecular =
			Material::LoadForModel(specularTexture, parentDirectory);
		if (material->TextureSpecular) {
			material->TextureSpecularName =
				StringUtils::Duplicate(material->TextureSpecular->Filename);
		}

		Memory::Free(specularTexture);
	}

	// Read ambient
	if (flags & 4) {
		char* ambientTexture;
		Uint8 ambientColor[4];

		ReadMaterialInfo(stream, ambientColor, &ambientTexture);

		ColorUtils::Separate(material->ColorAmbient, ambientColor);

		material->TextureAmbient = Material::LoadForModel(ambientTexture, parentDirectory);
		if (material->TextureAmbient) {
			material->TextureAmbientName =
				StringUtils::Duplicate(material->TextureAmbient->Filename);
		}

		Memory::Free(ambientTexture);
	}

	// Read emissive
	if (flags & 8) {
		char* emissiveTexture;
		Uint8 emissiveColor[4];

		ReadMaterialInfo(stream, emissiveColor, &emissiveTexture);

		ColorUtils::Separate(material->ColorEmissive, emissiveColor);

		material->TextureEmissive =
			Material::LoadForModel(emissiveTexture, parentDirectory);
		if (material->TextureEmissive) {
			material->TextureEmissiveName =
				StringUtils::Duplicate(material->TextureEmissive->Filename);
		}

		Memory::Free(emissiveTexture);
	}

	// Read shininess
	if (flags & 16) {
		Uint8 val = stream->ReadByte();
		material->Shininess = (float)val / 255;
		val = stream->ReadByte();
		material->ShininessStrength = (float)val / 255;
	}

	// Read opacity
	if (flags & 32) {
		Uint8 val = stream->ReadByte();
		material->Opacity = (float)val / 255;
	}

	return material;
}

void HatchModel::ReadVertexStore(Stream* stream) {
	NumVertexStore = stream->ReadUInt32();
	if (!NumVertexStore) {
		return;
	}

	VertexStore = (Vector3*)Memory::Malloc(NumVertexStore * sizeof(Vector3));

	Vector3* vert = VertexStore;
	for (Uint32 i = 0; i < NumVertexStore; i++) {
		vert->X = stream->ReadInt64();
		vert->Y = stream->ReadInt64();
		vert->Z = stream->ReadInt64();
		vert++;
	}
}

void HatchModel::ReadNormalStore(Stream* stream) {
	NumNormalStore = stream->ReadUInt32();
	if (!NumNormalStore) {
		return;
	}

	NormalStore = (Vector3*)Memory::Malloc(NumNormalStore * sizeof(Vector3));

	Vector3* norm = NormalStore;
	for (Uint32 i = 0; i < NumNormalStore; i++) {
		norm->X = stream->ReadInt64();
		norm->Y = stream->ReadInt64();
		norm->Z = stream->ReadInt64();
		norm++;
	}
}

void HatchModel::ReadTexCoordStore(Stream* stream) {
	NumTexCoordStore = stream->ReadUInt32();
	if (!NumTexCoordStore) {
		return;
	}

	TexCoordStore = (Vector2*)Memory::Malloc(NumTexCoordStore * sizeof(Vector2));

	Vector2* uvs = TexCoordStore;
	for (Uint32 i = 0; i < NumTexCoordStore; i++) {
		uvs->X = stream->ReadInt64();
		uvs->Y = stream->ReadInt64();
		uvs++;
	}
}

void HatchModel::ReadColorStore(Stream* stream) {
	NumColorStore = stream->ReadUInt32();
	if (!NumColorStore) {
		return;
	}

	ColorStore = (Uint32*)Memory::Malloc(NumColorStore * sizeof(Uint32));

	Uint32* colors = ColorStore;
	for (Uint32 i = 0; i < NumColorStore; i++) {
		Uint8 r = stream->ReadByte();
		Uint8 g = stream->ReadByte();
		Uint8 b = stream->ReadByte();
		Uint8 a = stream->ReadByte();
		*colors = ColorUtils::ToRGB(r, g, b, a);
		colors++;
	}
}

Vector3 HatchModel::GetStoredVertex(Uint32 idx) {
	if (idx < 0 || idx >= NumVertexStore) {
		return {};
	}

	return VertexStore[idx];
}

Vector3 HatchModel::GetStoredNormal(Uint32 idx) {
	if (idx < 0 || idx >= NumNormalStore) {
		return {};
	}

	return NormalStore[idx];
}

Vector2 HatchModel::GetStoredTexCoord(Uint32 idx) {
	if (idx < 0 || idx >= NumTexCoordStore) {
		return {};
	}

	return TexCoordStore[idx];
}

Uint32 HatchModel::GetStoredColor(Uint32 idx) {
	if (idx < 0 || idx >= NumColorStore) {
		return 0;
	}

	return ColorStore[idx];
}

void HatchModel::ReadVertexIndices(Sint32* indices, Uint32 triangleCount, Stream* stream) {
	for (Uint32 i = 0; i < triangleCount; i++) {
		*indices++ = stream->ReadUInt32();
		*indices++ = stream->ReadUInt32();
		*indices++ = stream->ReadUInt32();
	}

	*indices = -1;
}

Mesh* HatchModel::ReadMesh(IModel* model, Stream* stream) {
	// Read mesh data
	Mesh* mesh = new Mesh;
	mesh->Name = stream->ReadString();
	mesh->VertexFlag = VertexType_Position;

	Uint8 flags = stream->ReadByte();
	if (flags & 1) {
		mesh->VertexFlag |= VertexType_Normal;
	}
	if (flags & 2) {
		mesh->VertexFlag |= VertexType_UV;
	}
	if (flags & 4) {
		mesh->VertexFlag |= VertexType_Color;
	}
	if (flags & 8) {
		mesh->MaterialIndex = stream->ReadByte();
	}

	Uint32 vertexCount = stream->ReadUInt32();
	if (!vertexCount) {
		Log::Print(Log::LOG_ERROR, "Mesh \"%s\" has no vertices!", mesh->Name);
		return nullptr;
	}

	Uint32 triangleCount = stream->ReadUInt32();
	if (!triangleCount) {
		Log::Print(Log::LOG_ERROR, "Mesh \"%s\" has no triangles!", mesh->Name);
		return nullptr;
	}

	Uint16 frameCount = stream->ReadUInt16();
	if (!frameCount) {
		Log::Print(Log::LOG_ERROR, "Mesh \"%s\" has no frames!", mesh->Name);
		return nullptr;
	}

	mesh->VertexCount = vertexCount;
	mesh->FrameCount = frameCount;

	mesh->PositionBuffer = (Vector3*)Memory::Malloc(vertexCount * frameCount * sizeof(Vector3));

	// Read vertices
	Vector3* vert = mesh->PositionBuffer;
	for (Uint16 i = 0; i < frameCount; i++) {
		for (Uint32 j = 0; j < vertexCount; j++) {
			*vert++ = GetStoredVertex(stream->ReadUInt32());
		}
	}

	// Read normals
	if (mesh->VertexFlag & VertexType_Normal) {
		mesh->NormalBuffer =
			(Vector3*)Memory::Malloc(vertexCount * frameCount * sizeof(Vector3));

		Vector3* norm = mesh->NormalBuffer;
		for (Uint16 i = 0; i < frameCount; i++) {
			for (Uint32 j = 0; j < vertexCount; j++) {
				*norm++ = GetStoredNormal(stream->ReadUInt32());
			}
		}
	}

	// Read UVs
	if (mesh->VertexFlag & VertexType_UV) {
		mesh->UVBuffer =
			(Vector2*)Memory::Malloc(vertexCount * frameCount * sizeof(Vector2));

		Vector2* uv = mesh->UVBuffer;
		for (Uint16 i = 0; i < frameCount; i++) {
			for (Uint32 j = 0; j < vertexCount; j++) {
				*uv++ = GetStoredTexCoord(stream->ReadUInt32());
			}
		}
	}

	// Read colors
	if (mesh->VertexFlag & VertexType_Color) {
		mesh->ColorBuffer =
			(Uint32*)Memory::Malloc(vertexCount * frameCount * sizeof(Uint32));

		Uint32* colors = mesh->ColorBuffer;
		for (Uint16 i = 0; i < frameCount; i++) {
			for (Uint32 j = 0; j < vertexCount; j++) {
				*colors++ = GetStoredColor(stream->ReadUInt32());
			}
		}
	}

	// Read vertex indices
	mesh->VertexIndexCount = triangleCount * 3;
	model->VertexIndexCount += mesh->VertexIndexCount;

	mesh->VertexIndexBuffer =
		(Sint32*)Memory::Malloc((mesh->VertexIndexCount + 1) * sizeof(Sint32));

	ReadVertexIndices(mesh->VertexIndexBuffer, triangleCount, stream);

	return mesh;
}

bool HatchModel::Convert(IModel* model, Stream* stream, const char* path) {
	bool success = false;

	if (stream->ReadUInt32BE() != HATCH_MODEL_MAGIC) {
		Log::Print(Log::LOG_ERROR, "Model not of Hatch type!");
		return false;
	}

	Version = stream->ReadByte();

	if (Version > 0) {
		Log::Print(Log::LOG_ERROR, "Unsupported Hatch model version!");
		return false;
	}

	Uint16 meshCount = stream->ReadUInt16();
	if (!meshCount) {
		Log::Print(Log::LOG_ERROR, "Model has no meshes!");
		return false;
	}

	Uint8 materialCount;
	Uint16 animCount;

	Uint32 vertexDataOffset = stream->ReadUInt32();
	Uint32 normalDataOffset = stream->ReadUInt32();
	Uint32 uvDataOffset = stream->ReadUInt32();
	Uint32 colorDataOffset = stream->ReadUInt32();
	Uint32 meshDataOffset = stream->ReadUInt32();
	Uint32 materialDataOffset = stream->ReadUInt32();
	Uint32 animDataOffset = stream->ReadUInt32();

	// Read vertex storage
	stream->Seek(vertexDataOffset);
	ReadVertexStore(stream);

	// Read normal storage
	stream->Seek(normalDataOffset);
	ReadNormalStore(stream);

	// Read UV storage
	stream->Seek(uvDataOffset);
	ReadTexCoordStore(stream);

	// Read color storage
	stream->Seek(colorDataOffset);
	ReadColorStore(stream);

	// Create model
	model->VertexCount = 0;
	model->VertexPerFace = 3;
	model->UseVertexAnimation = true;

	// Read materials
	stream->Seek(materialDataOffset);

	materialCount = stream->ReadByte();

	if (materialCount) {
		char* parentDirectory = StringUtils::GetPath(path);

		for (Uint8 i = 0; i < materialCount; i++) {
			Material* material = ReadMaterial(stream, parentDirectory);
			if (material == nullptr) {
				for (unsigned j = 0; j <= i; j++) {
					delete model->Materials[j];
				}

				model->Materials.clear();

				goto fail;
			}

			model->AddUniqueMaterial(material);
		}

		Memory::Free(parentDirectory);
	}

	// Read animations
	stream->Seek(animDataOffset);

	animCount = stream->ReadUInt16();

	if (animCount) {
		for (Uint16 i = 0; i < animCount; i++) {
			ModelAnim* anim = new ModelAnim;
			anim->Name = stream->ReadString();
			anim->StartFrame = stream->ReadUInt32();
			anim->Length = stream->ReadUInt32();
			model->Animations.push_back(anim);
		}
	}

	// Read meshes
	stream->Seek(meshDataOffset);

	for (Uint16 i = 0; i < meshCount; i++) {
		Mesh* mesh = ReadMesh(model, stream);
		if (mesh == nullptr) {
			for (unsigned j = 0; j <= i; j++) {
				delete model->Meshes[j];
			}

			model->Meshes.clear();

			goto fail;
		}

		model->Meshes.push_back(mesh);
		model->VertexCount += mesh->VertexCount;
	}

	success = true;

fail:
	Memory::Free(VertexStore);
	Memory::Free(NormalStore);
	Memory::Free(TexCoordStore);
	Memory::Free(ColorStore);

	VertexStore = nullptr;
	NormalStore = nullptr;
	TexCoordStore = nullptr;
	ColorStore = nullptr;

	return success;
}

static HashMap<Uint32>* vertexIDs;
static HashMap<Uint32>* normalIDs;
static HashMap<Uint32>* texCoordIDs;
static HashMap<Uint32>* colorIDs;

static vector<Vector3> vertexList;
static vector<Vector3> normalList;
static vector<Vector2> texCoordList;
static vector<Uint32> colorList;

void HatchModel::WriteMesh(Mesh* mesh, Stream* stream) {
	Uint8 flags = 0;
	if (mesh->VertexFlag & VertexType_Normal) {
		flags |= 1;
	}
	if (mesh->VertexFlag & VertexType_UV) {
		flags |= 2;
	}
	if (mesh->VertexFlag & VertexType_Color) {
		flags |= 4;
	}
	if (mesh->MaterialIndex != -1) {
		flags |= 8;
	}

	stream->WriteString(mesh->Name);
	stream->WriteByte(flags);

	if (flags & 8) {
		stream->WriteByte(mesh->MaterialIndex);
	}

	Uint32 vertexCount = mesh->VertexCount;
	Uint16 frameCount = mesh->FrameCount;

	stream->WriteUInt32(vertexCount);
	stream->WriteUInt32(mesh->VertexIndexCount / 3);
	stream->WriteUInt16(frameCount);

	// Write vertices
	Vector3* vert = mesh->PositionBuffer;
	for (Uint16 j = 0; j < frameCount; j++) {
		for (Uint32 k = 0; k < vertexCount; k++) {
			Uint32 key = vertexIDs->HashFunction(vert, sizeof(*vert));
			if (vertexIDs->Exists(key)) {
				stream->WriteUInt32(vertexIDs->Get(key));
			}
			else {
				vertexIDs->Put(key, (Uint32)(vertexList.size()));
				stream->WriteUInt32(vertexList.size());
				vertexList.push_back(*vert);
			}
			vert++;
		}
	}

	// Write normals
	if (flags & 1) {
		Vector3* norm = mesh->NormalBuffer;
		for (Uint16 j = 0; j < frameCount; j++) {
			for (Uint32 k = 0; k < vertexCount; k++) {
				Uint32 key = normalIDs->HashFunction(norm, sizeof(*norm));
				if (normalIDs->Exists(key)) {
					stream->WriteUInt32(normalIDs->Get(key));
				}
				else {
					normalIDs->Put(key, (Uint32)(normalList.size()));
					stream->WriteUInt32(normalList.size());
					normalList.push_back(*norm);
				}
				norm++;
			}
		}
	}

	// Write texture coordinates
	if (flags & 2) {
		Vector2* uv = mesh->UVBuffer;
		for (Uint16 j = 0; j < frameCount; j++) {
			for (Uint32 k = 0; k < vertexCount; k++) {
				Uint32 key = texCoordIDs->HashFunction(uv, sizeof(*uv));
				if (texCoordIDs->Exists(key)) {
					stream->WriteUInt32(texCoordIDs->Get(key));
				}
				else {
					texCoordIDs->Put(key, (Uint32)(texCoordList.size()));
					stream->WriteUInt32(texCoordList.size());
					texCoordList.push_back(*uv);
				}
				uv++;
			}
		}
	}

	// Write colors
	if (flags & 4) {
		Uint32* color = mesh->ColorBuffer;
		for (Uint16 j = 0; j < frameCount; j++) {
			for (Uint32 k = 0; k < vertexCount; k++) {
				HatchModel::WriteColorIndex(color, stream);
				color++;
			}
		}
	}

	// Write vertex indices
	for (Uint32 j = 0; j < mesh->VertexIndexCount; j++) {
		stream->WriteUInt32(mesh->VertexIndexBuffer[j]);
	}
}

char* HatchModel::GetMaterialTextureName(const char* name, const char* parentDirectory) {
	const char* result = strstr(name, parentDirectory);
	if (result) {
		size_t offset = strlen(parentDirectory);
		while (name[offset] == '/') {
			offset++;
		}
		return StringUtils::Create(name + offset);
	}

	return StringUtils::Duplicate(name);
}

void HatchModel::WriteColorIndex(Uint32* color, Stream* stream) {
	Uint32 key = colorIDs->HashFunction(color, sizeof(*color));
	if (colorIDs->Exists(key)) {
		stream->WriteUInt32(colorIDs->Get(key));
	}
	else {
		colorIDs->Put(key, (Uint32)(colorList.size()));
		stream->WriteUInt32(colorList.size());
		colorList.push_back(*color);
	}
}

void HatchModel::WriteMaterial(Material* material, Stream* stream, const char* parentDirectory) {
	stream->WriteString(material->Name);

	Uint8 flags = 0;

	if ((material->ColorDiffuse[0] != 1.0 && material->ColorDiffuse[1] != 1.0 &&
		    material->ColorDiffuse[2] != 1.0 && material->ColorDiffuse[3] != 1.0) ||
		material->TextureDiffuseName) {
		flags |= 1;
	}

	if ((material->ColorSpecular[0] != 1.0 && material->ColorSpecular[1] != 1.0 &&
		    material->ColorSpecular[2] != 1.0 && material->ColorSpecular[3] != 1.0) ||
		material->TextureSpecularName) {
		flags |= 2;
	}

	if ((material->ColorAmbient[0] != 1.0 && material->ColorAmbient[1] != 1.0 &&
		    material->ColorAmbient[2] != 1.0 && material->ColorAmbient[3] != 1.0) ||
		material->TextureAmbientName) {
		flags |= 4;
	}

	if ((material->ColorEmissive[0] != 1.0 && material->ColorEmissive[1] != 1.0 &&
		    material->ColorEmissive[2] != 1.0 && material->ColorEmissive[3] != 1.0) ||
		material->TextureEmissiveName) {
		flags |= 8;
	}

	if (material->Shininess != 0.0 || material->ShininessStrength != 1.0) {
		flags |= 16;
	}

	if (material->Opacity != 1.0) {
		flags |= 32;
	}

	Log::Print(Log::LOG_VERBOSE, "Material: %s", material->Name);
	Log::Print(Log::LOG_VERBOSE, " - Flags: %d", flags);

	stream->WriteByte(flags);

	// Write diffuse
	if (flags & 1) {
		if (material->TextureDiffuseName) {
			char* name = GetMaterialTextureName(
				material->TextureDiffuseName, parentDirectory);

			stream->WriteString(name);

			Log::Print(Log::LOG_VERBOSE, " - Diffuse texture: %s", name);

			Memory::Free(name);
		}
		else {
			stream->WriteByte('\0');
		}

		Uint32 color = ColorUtils::ToRGBA(material->ColorDiffuse);

		HatchModel::WriteColorIndex(&color, stream);

		Log::Print(Log::LOG_VERBOSE,
			" - Diffuse color: %f %f %f %f",
			material->ColorDiffuse[0],
			material->ColorDiffuse[1],
			material->ColorDiffuse[2],
			material->ColorDiffuse[3]);
	}

	// Write specular
	if (flags & 2) {
		if (material->TextureSpecularName) {
			char* name = GetMaterialTextureName(
				material->TextureSpecularName, parentDirectory);

			stream->WriteString(name);

			Log::Print(Log::LOG_VERBOSE, " - Specular texture: %s", name);

			Memory::Free(name);
		}
		else {
			stream->WriteByte('\0');
		}

		Uint32 color = ColorUtils::ToRGBA(material->ColorSpecular);

		HatchModel::WriteColorIndex(&color, stream);

		Log::Print(Log::LOG_VERBOSE,
			" - Specular color: %f %f %f %f",
			material->ColorSpecular[0],
			material->ColorSpecular[1],
			material->ColorSpecular[2],
			material->ColorSpecular[3]);
	}

	// Write ambient
	if (flags & 4) {
		if (material->TextureAmbientName) {
			char* name = GetMaterialTextureName(
				material->TextureAmbientName, parentDirectory);

			stream->WriteString(name);

			Log::Print(Log::LOG_VERBOSE, " - Ambient texture: %s", name);

			Memory::Free(name);
		}
		else {
			stream->WriteByte('\0');
		}

		Uint32 color = ColorUtils::ToRGBA(material->ColorAmbient);

		HatchModel::WriteColorIndex(&color, stream);

		Log::Print(Log::LOG_VERBOSE,
			" - Ambient color: %f %f %f %f",
			material->ColorAmbient[0],
			material->ColorAmbient[1],
			material->ColorAmbient[2],
			material->ColorAmbient[3]);
	}

	// Write emissive
	if (flags & 8) {
		if (material->TextureEmissiveName) {
			char* name = GetMaterialTextureName(
				material->TextureEmissiveName, parentDirectory);

			stream->WriteString(name);

			Log::Print(Log::LOG_VERBOSE, " - Emissive texture: %s", name);

			Memory::Free(name);
		}
		else {
			stream->WriteByte('\0');
		}

		Uint32 color = ColorUtils::ToRGBA(material->ColorEmissive);

		HatchModel::WriteColorIndex(&color, stream);

		Log::Print(Log::LOG_VERBOSE,
			" - Emissive color: %f %f %f %f",
			material->ColorEmissive[0],
			material->ColorEmissive[1],
			material->ColorEmissive[2],
			material->ColorEmissive[3]);
	}

	// Write shininess
	if (flags & 16) {
		stream->WriteByte(material->Shininess * 0xFF);
		stream->WriteByte(material->ShininessStrength * 0xFF);

		Log::Print(Log::LOG_VERBOSE, " - Shininess: %f", material->Shininess);
		Log::Print(
			Log::LOG_VERBOSE, " - Shininess strength: %f", material->ShininessStrength);
	}

	// Write opacity
	if (flags & 32) {
		stream->WriteByte(material->Opacity * 0xFF);

		Log::Print(Log::LOG_VERBOSE, " - Opacity: %f", material->Opacity);
	}
}

bool HatchModel::Save(IModel* model, const char* filename) {
	Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Couldn't open \"%s\"!", filename);
		return false;
	}

	stream->WriteUInt32BE(HATCH_MODEL_MAGIC);
	stream->WriteByte(0);

	stream->WriteUInt16(model->Meshes.size());

	Uint32 vertexDataOffsetPos = stream->Position();
	stream->WriteUInt32(0x00000000);
	Uint32 normalDataOffsetPos = stream->Position();
	stream->WriteUInt32(0x00000000);
	Uint32 uvDataOffsetPos = stream->Position();
	stream->WriteUInt32(0x00000000);
	Uint32 colorDataOffsetPos = stream->Position();
	stream->WriteUInt32(0x00000000);
	Uint32 meshDataOffsetPos = stream->Position();
	stream->WriteUInt32(0x00000000);
	Uint32 materialDataOffsetPos = stream->Position();
	stream->WriteUInt32(0x00000000);
	Uint32 animDataOffsetPos = stream->Position();
	stream->WriteUInt32(0x00000000);

	vertexIDs = new HashMap<Uint32>(NULL, 1024);
	normalIDs = new HashMap<Uint32>(NULL, 1024);
	texCoordIDs = new HashMap<Uint32>(NULL, 1024);
	colorIDs = new HashMap<Uint32>(NULL, 1024);

	vertexList.clear();
	normalList.clear();
	texCoordList.clear();
	colorList.clear();

	// Write meshes
	size_t lastPos = stream->Position();
	stream->Seek(meshDataOffsetPos);
	stream->WriteUInt32(lastPos);
	stream->Seek(lastPos);

	Log::Print(Log::LOG_VERBOSE, "Mesh count: %d (%08X)", model->Meshes.size(), lastPos);

	for (size_t i = 0; i < model->Meshes.size(); i++) {
		WriteMesh(model->Meshes[i], stream);
	}

	// Write materials
	lastPos = stream->Position();
	stream->Seek(materialDataOffsetPos);
	stream->WriteUInt32(lastPos);
	stream->Seek(lastPos);

	size_t numMaterials = model->Materials.size();

	stream->WriteByte(numMaterials);

	Log::Print(Log::LOG_VERBOSE, "Material count: %d (%08X)", numMaterials, lastPos);

	char* parentDirectory = StringUtils::GetPath(filename);
	char* parentDirectoryPtr = parentDirectory;
	if (StringUtils::StartsWith(parentDirectoryPtr, "./")) {
		parentDirectoryPtr += 2;
	}
	if (StringUtils::StartsWith(parentDirectoryPtr, "Resources/")) {
		parentDirectoryPtr += 10;
	}

	for (size_t i = 0; i < numMaterials; i++) {
		WriteMaterial(model->Materials[i], stream, parentDirectoryPtr);
	}

	Memory::Free(parentDirectory);

	// Write animations
	lastPos = stream->Position();
	stream->Seek(animDataOffsetPos);
	stream->WriteUInt32(lastPos);
	stream->Seek(lastPos);

	size_t numAnimations = model->Animations.size();

	stream->WriteByte(numAnimations);

	Log::Print(Log::LOG_VERBOSE, "Animation count: %d (%08X)", numAnimations, lastPos);

	for (size_t i = 0; i < numAnimations; i++) {
		ModelAnim* anim = model->Animations[i];
		stream->WriteString(anim->Name);
		stream->WriteUInt32(anim->StartFrame);
		stream->WriteUInt32(anim->Length);
	}

	// Write stored vertices
	lastPos = stream->Position();
	stream->Seek(vertexDataOffsetPos);
	stream->WriteUInt32(lastPos);
	stream->Seek(lastPos);

	size_t numVertices = vertexList.size();

	stream->WriteUInt32(numVertices);

	Log::Print(Log::LOG_VERBOSE, "Vertex list size: %d (%08X)", numVertices, lastPos);

	for (size_t i = 0; i < numVertices; i++) {
		Vector3 v = vertexList[i];
		stream->WriteInt64(v.X);
		stream->WriteInt64(v.Y);
		stream->WriteInt64(v.Z);
	}

	// Write stored normals
	lastPos = stream->Position();
	stream->Seek(normalDataOffsetPos);
	stream->WriteUInt32(lastPos);
	stream->Seek(lastPos);

	size_t numNormals = normalList.size();

	stream->WriteUInt32(numNormals);

	Log::Print(Log::LOG_VERBOSE, "Normal list size: %d (%08X)", numNormals, lastPos);

	for (size_t i = 0; i < numNormals; i++) {
		Vector3 n = normalList[i];
		stream->WriteInt64(n.X);
		stream->WriteInt64(n.Y);
		stream->WriteInt64(n.Z);
	}

	// Write stored texture coordinates
	lastPos = stream->Position();
	stream->Seek(uvDataOffsetPos);
	stream->WriteUInt32(lastPos);
	stream->Seek(lastPos);

	size_t numTexCoords = texCoordList.size();

	stream->WriteUInt32(numTexCoords);

	Log::Print(
		Log::LOG_VERBOSE, "Texture coordinate list size: %d (%08X)", numTexCoords, lastPos);

	for (size_t i = 0; i < numTexCoords; i++) {
		Vector2 t = texCoordList[i];
		stream->WriteInt64(t.X);
		stream->WriteInt64(t.Y);
	}

	// Write stored colors
	lastPos = stream->Position();
	stream->Seek(colorDataOffsetPos);
	stream->WriteUInt32(lastPos);
	stream->Seek(lastPos);

	size_t numColors = colorList.size();

	stream->WriteUInt32(numColors);

	Log::Print(Log::LOG_VERBOSE, "Color list size: %d (%08X)", numColors, lastPos);

	for (size_t i = 0; i < numColors; i++) {
		Uint32 color = colorList[i];
		Uint8 rgba[4];
		ColorUtils::Separate(color, rgba);
		stream->WriteByte(rgba[0]);
		stream->WriteByte(rgba[1]);
		stream->WriteByte(rgba[2]);
		stream->WriteByte(rgba[3]);
	}

	stream->Close();

	delete vertexIDs;
	delete normalIDs;
	delete texCoordIDs;
	delete colorIDs;

	return true;
}
