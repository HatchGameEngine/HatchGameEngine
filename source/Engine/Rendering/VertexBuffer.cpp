#include <Engine/Diagnostics/Memory.h>
#include <Engine/Rendering/VertexBuffer.h>

VertexBuffer::VertexBuffer() {}
VertexBuffer::VertexBuffer(Uint32 numVertices) {
	Init(numVertices);
	Clear();
}
void VertexBuffer::Init(Uint32 numVertices) {
	if (!numVertices) {
		numVertices = 192;
	}

	if (Capacity != numVertices) {
		Resize(numVertices);
	}
}
void VertexBuffer::Clear() {
	VertexCount = 0;
	FaceCount = 0;

	// Not sure why we do this
	memset(Vertices, 0x00, Capacity * sizeof(VertexAttribute));
}
void VertexBuffer::Resize(Uint32 numVertices) {
	if (!numVertices) {
		numVertices = 192;
	}

	Capacity = numVertices;
	Vertices =
		(VertexAttribute*)Memory::Realloc(Vertices, numVertices * sizeof(VertexAttribute));
	FaceInfoBuffer = (FaceInfo*)Memory::Realloc(
		FaceInfoBuffer, ((numVertices / 3) + 1) * sizeof(FaceInfo));
}
VertexBuffer::~VertexBuffer() {
	Memory::Free(Vertices);
	Memory::Free(FaceInfoBuffer);
}
