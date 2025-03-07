#ifndef VECTORTYPES_H
#define VECTORTYPES_H

#include "Engine/Includes/Standard.h"

struct Vector2 {
	Sint64 X;
	Sint64 Y;
};
struct Vector3 {
	Sint64 X;
	Sint64 Y;
	Sint64 Z;
};
struct Vector4 {
	Sint64 X;
	Sint64 Y;
	Sint64 Z;
	Sint64 W;
};

struct FVector2 {
	float X = 0.0;
	float Y = 0.0;

	FVector2() {}

	FVector2(float x, float y) {
		X = x;
		Y = y;
	}

	FVector2 operator+(const FVector2& b) {
		FVector2 result(X + b.X, Y + b.Y);
		return result;
	}

	FVector2 operator-(const FVector2& b) {
		FVector2 result(X - b.X, Y - b.Y);
		return result;
	}

	FVector2 operator-() { return FVector2(-X, -Y); }

	bool operator==(const FVector2& rhs) { return X == rhs.X && Y == rhs.Y; }

	bool operator!=(const FVector2& rhs) { return !(*this == rhs); }

	static float CrossProduct(FVector2 v1, FVector2 v2, FVector2 v3) {
		FVector2 va = v2 - v1;
		FVector2 vb = v3 - v2;
		return Determinant(va, vb);
	}

	static float DotProduct(FVector2 v1, FVector2 v2) { return (v1.X * v2.X) + (v1.Y * v2.Y); }

	static float Determinant(FVector2 v1, FVector2 v2) { return (v1.X * v2.Y) - (v1.Y * v2.X); }
};

struct FLineSegment {
	FVector2 A, B;

	FLineSegment(FVector2 a, FVector2 b) {
		A = a;
		B = b;
	}

	FLineSegment(float x1, float y1, float x2, float y2) {
		A.X = x1;
		A.Y = y1;
		B.X = x2;
		B.Y = y2;
	}

	static bool DoIntersection(FVector2 line1A,
		FVector2 line1B,
		FVector2 line2A,
		FVector2 line2B,
		FVector2& result) {
		float dx1 = line1B.X - line1A.X;
		float dy1 = line1B.Y - line1A.Y;
		float dx2 = line2B.X - line2A.X;
		float dy2 = line2B.Y - line2A.Y;
		float d = dx1 * dy2 - dy1 * dx2;
		if (d == 0.0) {
			return false;
		}

		float n = ((line1A.Y - line2A.Y) * dx2) - ((line1A.X - line2A.X) * dy2);
		float t = n / d;
		if (t < 0.0 || t > 1.0) {
			return false;
		}

		float n2 = ((line1A.Y - line2A.Y) * dx1) - ((line1A.X - line2A.X) * dy1);
		float t2 = n2 / d;
		if (t2 < 0.0 || t2 > 1.0) {
			return false;
		}

		result.X = line1A.X + (t * dx1);
		result.Y = line1A.Y + (t * dy1);

		return true;
	}

	bool DoIntersection(FVector2 lineA, FVector2 lineB, FVector2& result) {
		return FLineSegment::DoIntersection(A, B, lineA, lineB, result);
	}
};

#endif /* VECTORTYPES_H */
