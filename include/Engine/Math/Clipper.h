#ifndef ENGINE_MATH_CLIPPER_H
#define ENGINE_MATH_CLIPPER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>

class Clipper {
private:
	static void AddPoint(VertexAttribute* buf,
		VertexAttribute* v1,
		VertexAttribute* v2,
		Vector4 p1,
		Vector4 p2,
		Sint64 t);
	static bool ClipEdge(Frustum frustum,
		VertexAttribute* v1,
		VertexAttribute* v2,
		PolygonClipBuffer* output);
	static int ClipPolygon(Frustum frustum,
		PolygonClipBuffer* output,
		VertexAttribute* input,
		int vertexCount);

public:
	static int FrustumClip(PolygonClipBuffer* output,
		Frustum* frustum,
		int num,
		VertexAttribute* input,
		int vertexCount);
};

#endif /* ENGINE_MATH_CLIPPER_H */
