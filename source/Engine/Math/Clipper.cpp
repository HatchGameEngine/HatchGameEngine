#include <Engine/Math/Clipper.h>
#include <Engine/Math/Vector.h>

void Clipper::AddPoint(VertexAttribute* buf,
	VertexAttribute* v1,
	VertexAttribute* v2,
	Vector4 p1,
	Vector4 p2,
	Sint64 t) {
	Vector4 diff = Vector::Subtract(p2, p1);
	Vector4 newPosition = Vector::Add(p1, Vector::Multiply(diff, t));

	buf->Position = newPosition;

	float x = (float)t / 0x10000;

#define DO_INTERP(field) (x * (v2->field - v1->field)) + v1->field;

	buf->Normal.X = DO_INTERP(Normal.X);
	buf->Normal.Y = DO_INTERP(Normal.Y);
	buf->Normal.Z = DO_INTERP(Normal.Z);
	buf->Normal.W = DO_INTERP(Normal.W);
	buf->Color = DO_INTERP(Color);
	buf->UV.X = DO_INTERP(UV.X);
	buf->UV.Y = DO_INTERP(UV.Y);

#undef DO_INTERP
}

bool Clipper::ClipEdge(Frustum frustum,
	VertexAttribute* v1,
	VertexAttribute* v2,
	PolygonClipBuffer* output) {
	VertexAttribute* buffer = &output->Buffer[output->NumPoints];

	Vector4 pos1 = v1->Position;
	Vector4 pos2 = v2->Position;

	frustum.Normal = Vector::Normalize(frustum.Normal);

	Sint64 distance1 = Vector::DistanceToPlane(pos1, frustum.Plane, frustum.Normal);
	Sint64 distance2 = Vector::DistanceToPlane(pos2, frustum.Plane, frustum.Normal);

	bool inside1 = distance1 >= 0;
	bool inside2 = distance2 >= 0;

	// If both points are inside, add the second point
	if (inside1 && inside2) {
		if (output->NumPoints == output->MaxPoints) {
			return false;
		}

		buffer->Position = v2->Position;
		buffer->Normal = v2->Normal;
		buffer->Color = v2->Color;
		buffer->UV = v2->UV;

		output->NumPoints++;
	}
	else if (inside1) {
		// If the first point is inside, add the intersected
		// point
		Sint64 t = Vector::IntersectWithPlane(frustum.Plane, frustum.Normal, pos1, pos2);
		if (t <= 0) {
			return false;
		}

		Clipper::AddPoint(buffer, v1, v2, pos1, pos2, t);

		output->NumPoints++;
	}
	else if (inside2) {
		// If the second point is inside, add both the
		// intersected and the second point
		if (output->NumPoints + 1 == output->MaxPoints) {
			return false;
		}

		Sint64 t = Vector::IntersectWithPlane(frustum.Plane, frustum.Normal, pos2, pos1);
		if (t <= 0) {
			return false;
		}

		Clipper::AddPoint(buffer, v1, v2, pos2, pos1, t);

		buffer++;
		buffer->Position = v2->Position;
		buffer->Normal = v2->Normal;
		buffer->Color = v2->Color;
		buffer->UV = v2->UV;

		output->NumPoints += 2;
	}

	return true;
}

int Clipper::ClipPolygon(Frustum frustum,
	PolygonClipBuffer* output,
	VertexAttribute* input,
	int vertexCount) {
	// Not even a triangle?
	if (vertexCount < 3) {
		return 0;
	}

	VertexAttribute* lastVertex = input;
	int countRem = vertexCount - 1;
	while (countRem--) {
		if (!Clipper::ClipEdge(frustum, &lastVertex[0], &lastVertex[1], output)) {
			return 0;
		}

		lastVertex++;
	}

	if (!Clipper::ClipEdge(frustum, &lastVertex[0], &input[0], output)) {
		return 0;
	}

	return output->NumPoints;
}

int Clipper::FrustumClip(PolygonClipBuffer* output,
	Frustum* frustum,
	int num,
	VertexAttribute* input,
	int vertexCount) {
	PolygonClipBuffer temp[NUM_FRUSTUM_PLANES];

	VertexAttribute* buffer = input;

	for (int i = 0; i < num; i++) {
		temp[i].NumPoints = output->NumPoints;
		temp[i].MaxPoints = output->MaxPoints;

		vertexCount = Clipper::ClipPolygon(frustum[i], &temp[i], buffer, vertexCount);
		buffer = temp[i].Buffer; // pass the buck
	}

	memcpy(output->Buffer, buffer, sizeof(VertexAttribute) * vertexCount);

	return vertexCount;
}
