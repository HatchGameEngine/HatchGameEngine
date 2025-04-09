#ifndef ENGINE_MATH_VECTOR_H
#define ENGINE_MATH_VECTOR_H

#include <Engine/Includes/Standard.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Math/VectorTypes.h>

namespace Vector {
//public:
	Vector4 Add(Vector4 v1, Vector4 v2);
	Vector4 Subtract(Vector4 v1, Vector4 v2);
	Vector4 Multiply(Vector4 v, Sint64 t);
	Vector3 Multiply(Vector3 v, Matrix4x4* m);
	Vector2 Interpolate(Vector2 v1, Vector2 v2, Sint64 t);
	Vector3 Interpolate(Vector3 v1, Vector3 v2, Sint64 t);
	Vector4 Interpolate(Vector4 v1, Vector4 v2, Sint64 t);
	Sint64 DotProduct(Vector4 v1, Vector4 v2);
	Sint64 Length(Vector4 v);
	Vector4 Normalize(Vector4 v);
	Sint64 IntersectWithPlane(Vector4 plane, Vector4 normal, Vector4 v1, Vector4 v2);
	Sint64 DistanceToPlane(Vector4 v, Vector4 plane, Vector4 normal);
};

#endif /* ENGINE_MATH_VECTOR_H */
