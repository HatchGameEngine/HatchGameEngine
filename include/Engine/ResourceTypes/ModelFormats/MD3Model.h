#ifndef ENGINE_RESOURCETYPES_MODELFORMATS_MD3MODEL_H
#define ENGINE_RESOURCETYPES_MODELFORMATS_MD3MODEL_H

#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/IModel.h>

namespace MD3Model {
//private:
	void DecodeNormal(Uint16 index, float& x, float& y, float& z);
	void ReadShader(Stream* stream);
	void
	ReadVerticesAndNormals(Vector3* vert, Vector3* norm, Sint32 vertexCount, Stream* stream);
	void ReadUVs(Vector2* uvs, Sint32 vertexCount, Stream* stream);
	void ReadVertexIndices(Sint32* indices, Sint32 triangleCount, Stream* stream);
	Mesh* ReadSurface(IModel* model, Stream* stream, size_t surfaceDataOffset);

//public:
	extern Sint32 Version;
	extern bool UseUVKeyframes;
	extern Sint32 DataEndOffset;
	extern vector<string> MaterialNames;

	bool IsMagic(Stream* stream);
	bool Convert(IModel* model, Stream* stream, const char* path);
};

#endif /* ENGINE_RESOURCETYPES_MODELFORMATS_MD3MODEL_H */
