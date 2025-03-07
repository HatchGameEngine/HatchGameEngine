#ifndef ENGINE_RENDERING_VERTEXBUFFER_H
#define ENGINE_RENDERING_VERTEXBUFFER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/FaceInfo.h>

class VertexBuffer {
public:
	VertexAttribute* Vertices = nullptr; // count = max vertex count
	FaceInfo* FaceInfoBuffer = nullptr; // count = max face count
	void* DriverData = nullptr;
	Uint32 Capacity = 0;
	Uint32 VertexCount = 0;
	Uint32 FaceCount = 0;
	Uint32 UnloadPolicy;

	VertexBuffer();
	VertexBuffer(Uint32 numVertices);
	void Init(Uint32 numVertices);
	void Clear();
	void Resize(Uint32 numVertices);
	~VertexBuffer();
};

#endif /* ENGINE_RENDERING_VERTEXBUFFER_H */
