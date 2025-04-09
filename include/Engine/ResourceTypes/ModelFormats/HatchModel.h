#ifndef ENGINE_RESOURCETYPES_MODELFORMATS_HATCHMODEL_H
#define ENGINE_RESOURCETYPES_MODELFORMATS_HATCHMODEL_H

#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/IModel.h>

namespace HatchModel {
//private:
	Material* ReadMaterial(Stream* stream, const char* parentDirectory);
	void ReadVertexStore(Stream* stream);
	void ReadNormalStore(Stream* stream);
	void ReadTexCoordStore(Stream* stream);
	void ReadColorStore(Stream* stream);
	Vector3 GetStoredVertex(Uint32 idx);
	Vector3 GetStoredNormal(Uint32 idx);
	Vector2 GetStoredTexCoord(Uint32 idx);
	Uint32 GetStoredColor(Uint32 idx);
	void ReadVertexIndices(Sint32* indices, Uint32 triangleCount, Stream* stream);
	Mesh* ReadMesh(IModel* model, Stream* stream);
	void WriteMesh(Mesh* mesh, Stream* stream);
	char* GetMaterialTextureName(const char* name, const char* parentDirectory);
	void WriteColorIndex(Uint32* color, Stream* stream);
	void WriteMaterial(Material* material, Stream* stream, const char* parentDirectory);

//public:
	extern Sint32 Version;
	extern Uint32 NumVertexStore;
	extern Uint32 NumNormalStore;
	extern Uint32 NumTexCoordStore;
	extern Uint32 NumColorStore;
	extern Vector3* VertexStore;
	extern Vector3* NormalStore;
	extern Vector2* TexCoordStore;
	extern Uint32* ColorStore;

	bool IsMagic(Stream* stream);
	void ReadMaterialInfo(Stream* stream, Uint8* destColors, char** texName);
	bool Convert(IModel* model, Stream* stream, const char* path);
	bool Save(IModel* model, const char* filename);
};

#endif /* ENGINE_RESOURCETYPES_MODELFORMATS_HATCHMODEL_H */
