#include <Engine/Math/FixedPoint.h>
#include <Engine/Math/Vector.h>

Vector4 Vector::Add(Vector4 v1, Vector4 v2) {
	Vector4 result;
	result.X = v1.X + v2.X;
	result.Y = v1.Y + v2.Y;
	result.Z = v1.Z + v2.Z;
	result.W = v1.W + v2.W;
	return result;
}

Vector4 Vector::Subtract(Vector4 v1, Vector4 v2) {
	Vector4 result;
	result.X = v1.X - v2.X;
	result.Y = v1.Y - v2.Y;
	result.Z = v1.Z - v2.Z;
	result.W = v1.W - v2.W;
	return result;
}

Vector4 Vector::Multiply(Vector4 v, Sint64 t) {
	Vector4 result;
	result.X = FP16_MULTIPLY(v.X, t);
	result.Y = FP16_MULTIPLY(v.Y, t);
	result.Z = FP16_MULTIPLY(v.Z, t);
	result.W = FP16_MULTIPLY(v.W, t);
	return result;
}

Vector3 Vector::Multiply(Vector3 v, Matrix4x4* m) {
	Vector3 result;

	Sint64 mat11 = m->Values[0] * 0x10000;
	Sint64 mat12 = m->Values[1] * 0x10000;
	Sint64 mat13 = m->Values[2] * 0x10000;
	Sint64 mat14 = m->Values[3] * 0x10000;
	Sint64 mat21 = m->Values[4] * 0x10000;
	Sint64 mat22 = m->Values[5] * 0x10000;
	Sint64 mat23 = m->Values[6] * 0x10000;
	Sint64 mat24 = m->Values[7] * 0x10000;
	Sint64 mat31 = m->Values[8] * 0x10000;
	Sint64 mat32 = m->Values[9] * 0x10000;
	Sint64 mat33 = m->Values[10] * 0x10000;
	Sint64 mat34 = m->Values[11] * 0x10000;

	result.X = FP16_MULTIPLY(mat11, v.X) + FP16_MULTIPLY(mat12, v.Y) +
		FP16_MULTIPLY(mat13, v.Z) + mat14;
	result.Y = FP16_MULTIPLY(mat21, v.X) + FP16_MULTIPLY(mat22, v.Y) +
		FP16_MULTIPLY(mat23, v.Z) + mat24;
	result.Z = FP16_MULTIPLY(mat31, v.X) + FP16_MULTIPLY(mat32, v.Y) +
		FP16_MULTIPLY(mat33, v.Z) + mat34;

	return result;
}

Vector2 Vector::Interpolate(Vector2 v1, Vector2 v2, Sint64 t) {
	Vector2 result;

	result.X = v1.X + FP16_MULTIPLY(v2.X - v1.X, t);
	result.Y = v1.Y + FP16_MULTIPLY(v2.Y - v1.Y, t);

	return result;
}

Vector3 Vector::Interpolate(Vector3 v1, Vector3 v2, Sint64 t) {
	Vector3 result;

	result.X = v1.X + FP16_MULTIPLY(v2.X - v1.X, t);
	result.Y = v1.Y + FP16_MULTIPLY(v2.Y - v1.Y, t);
	result.Z = v1.Z + FP16_MULTIPLY(v2.Z - v1.Z, t);

	return result;
}

Vector4 Vector::Interpolate(Vector4 v1, Vector4 v2, Sint64 t) {
	Vector4 result;

	result.X = v1.X + FP16_MULTIPLY(v2.X - v1.X, t);
	result.Y = v1.Y + FP16_MULTIPLY(v2.Y - v1.Y, t);
	result.Z = v1.Z + FP16_MULTIPLY(v2.Z - v1.Z, t);
	result.W = v1.W + FP16_MULTIPLY(v2.W - v1.W, t);

	return result;
}

Sint64 Vector::DotProduct(Vector4 v1, Vector4 v2) {
	Sint64 result = 0;
	result += FP16_MULTIPLY(v1.X, v2.X);
	result += FP16_MULTIPLY(v1.Y, v2.Y);
	result += FP16_MULTIPLY(v1.Z, v2.Z);
	result += FP16_MULTIPLY(v1.W, v2.W);
	return result;
}

Sint64 Vector::Length(Vector4 v) {
	float result = Vector::DotProduct(v, v);
	result = sqrtf(result / 0x10000);
	return result * 0x10000;
}

Vector4 Vector::Normalize(Vector4 v) {
	Vector4 result;

	Sint64 length = Vector::Length(v);
	if (length == 0) {
		result.X = result.Y = result.Z = result.W = 0;
	}
	else {
		result.X = FP16_DIVIDE(v.X, length);
		result.Y = FP16_DIVIDE(v.Y, length);
		result.Z = FP16_DIVIDE(v.Z, length);
		result.W = FP16_DIVIDE(v.W, length);
	}

	return result;
}

Sint64 Vector::IntersectWithPlane(Vector4 plane, Vector4 normal, Vector4 v1, Vector4 v2) {
	Vector4 planeNormal = Vector::Normalize(normal);
	Sint64 dotProduct = Vector::DotProduct(v1, planeNormal);
	Sint64 t = Vector::DotProduct(planeNormal, plane) - dotProduct;
	Sint64 diff = Vector::DotProduct(v2, planeNormal) - dotProduct;
	if (diff == 0) {
		return 0;
	}

	return FP16_DIVIDE(t, diff);
}

Sint64 Vector::DistanceToPlane(Vector4 v, Vector4 plane, Vector4 normal) {
	Sint64 dotProduct = Vector::DotProduct(normal, plane);
	Sint64 result = 0;

	result += FP16_MULTIPLY(v.X, normal.X);
	result += FP16_MULTIPLY(v.Y, normal.Y);
	result += FP16_MULTIPLY(v.Z, normal.Z);
	result += FP16_MULTIPLY(v.W, normal.W);
	result -= dotProduct;

	return result;
}
