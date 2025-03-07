#ifndef ENGINE_RESOURCETYPES_MODELFORMATS_MD3MODEL_H
#define ENGINE_RESOURCETYPES_MODELFORMATS_MD3MODEL_H

#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/IModel.h>

class MD3Model {
private:
	static void DecodeNormal(Uint16 index, float& x, float& y, float& z);
	static void ReadShader(Stream* stream);
	static void
	ReadVerticesAndNormals(Vector3* vert, Vector3* norm, Sint32 vertexCount, Stream* stream);
	static void ReadUVs(Vector2* uvs, Sint32 vertexCount, Stream* stream);
	static void ReadVertexIndices(Sint32* indices, Sint32 triangleCount, Stream* stream);
	static Mesh* ReadSurface(IModel* model, Stream* stream, size_t surfaceDataOffset);

public:
	static Sint32 Version;
	static bool UseUVKeyframes;
	static Sint32 DataEndOffset;
	static vector<string> MaterialNames;

	static bool IsMagic(Stream* stream);
	static bool Convert(IModel* model, Stream* stream, const char* path);
};

#endif /* ENGINE_RESOURCETYPES_MODELFORMATS_MD3MODEL_H */
