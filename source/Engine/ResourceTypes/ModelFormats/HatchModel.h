#ifndef ENGINE_RESOURCETYPES_MODELFORMATS_HATCHMODEL_H
#define ENGINE_RESOURCETYPES_MODELFORMATS_HATCHMODEL_H

#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/IModel.h>

class HatchModel {
private:
	static Material* ReadMaterial(Stream* stream, const char* parentDirectory);
	static void ReadVertexStore(Stream* stream);
	static void ReadNormalStore(Stream* stream);
	static void ReadTexCoordStore(Stream* stream);
	static void ReadColorStore(Stream* stream);
	static Vector3 GetStoredVertex(Uint32 idx);
	static Vector3 GetStoredNormal(Uint32 idx);
	static Vector2 GetStoredTexCoord(Uint32 idx);
	static Uint32 GetStoredColor(Uint32 idx);
	static void ReadVertexIndices(Sint32* indices, Uint32 triangleCount, Stream* stream);
	static Mesh* ReadMesh(IModel* model, Stream* stream);
	static void WriteMesh(Mesh* mesh, Stream* stream);
	static char* GetMaterialTextureName(const char* name, const char* parentDirectory);
	static void WriteColorIndex(Uint32* color, Stream* stream);
	static void WriteMaterial(Material* material, Stream* stream, const char* parentDirectory);

public:
	static Sint32 Version;
	static Uint32 NumVertexStore;
	static Uint32 NumNormalStore;
	static Uint32 NumTexCoordStore;
	static Uint32 NumColorStore;
	static Vector3* VertexStore;
	static Vector3* NormalStore;
	static Vector2* TexCoordStore;
	static Uint32* ColorStore;

	static bool IsMagic(Stream* stream);
	static void ReadMaterialInfo(Stream* stream, Uint8* destColors, char** texName);
	static bool Convert(IModel* model, Stream* stream, const char* path);
	static bool Save(IModel* model, const char* filename);
};

#endif /* ENGINE_RESOURCETYPES_MODELFORMATS_HATCHMODEL_H */
