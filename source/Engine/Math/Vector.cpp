#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>

class Vector {
public:
};
#endif

#include <Engine/Math/Vector.h>

PUBLIC STATIC Vector4 Vector::Add(Vector4 v1, Vector4 v2) {
    Vector4 result;
    result.X = v1.X + v2.X;
    result.Y = v1.Y + v2.Y;
    result.Z = v1.Z + v2.Z;
    result.W = v1.W + v2.W;
    return result;
}

PUBLIC STATIC Vector4 Vector::Subtract(Vector4 v1, Vector4 v2) {
    Vector4 result;
    result.X = v1.X - v2.X;
    result.Y = v1.Y - v2.Y;
    result.Z = v1.Z - v2.Z;
    result.W = v1.W - v2.W;
    return result;
}

PUBLIC STATIC Vector4 Vector::Multiply(Vector4 v, int t) {
    Vector4 result;
    result.X = ((Sint64)v.X * t) >> 16;
    result.Y = ((Sint64)v.Y * t) >> 16;
    result.Z = ((Sint64)v.Z * t) >> 16;
    result.W = ((Sint64)v.W * t) >> 16;
    return result;
}

PUBLIC STATIC int     Vector::DotProduct(Vector4 v1, Vector4 v2) {
    int result = 0;
    result += ((Sint64)v1.X * v2.X) >> 16;
    result += ((Sint64)v1.Y * v2.Y) >> 16;
    result += ((Sint64)v1.Z * v2.Z) >> 16;
    result += ((Sint64)v1.W * v2.W) >> 16;
    return result;
}

PUBLIC STATIC int     Vector::Length(Vector4 v) {
    float result = Vector::DotProduct(v, v) / 0x10000;
    result = sqrtf(result);
    return result * 0x10000;
}

PUBLIC STATIC Vector4 Vector::Normalize(Vector4 v) {
    Vector4 result;

    int length = Vector::Length(v);
    if (length == 0)
        result.X = result.Y = result.Z = result.W = 0;
    else {
        result.X = ((Sint64)v.X * 0x10000) / length;
        result.Y = ((Sint64)v.Y * 0x10000) / length;
        result.Z = ((Sint64)v.Z * 0x10000) / length;
        result.W = ((Sint64)v.W * 0x10000) / length;
    }

    return result;
}

PUBLIC STATIC int     Vector::IntersectWithPlane(Vector4 plane, Vector4 normal, Vector4 v1, Vector4 v2) {
    Vector4 planeNormal = Vector::Normalize(normal);
    int dotProduct = Vector::DotProduct(v1, planeNormal);
    int t = Vector::DotProduct(planeNormal, plane) - dotProduct;
    int diff = Vector::DotProduct(v2, planeNormal) - dotProduct;
    if (diff == 0)
        return 0;

    return ((Sint64)t * 0x10000) / diff;
}

PUBLIC STATIC int     Vector::DistanceToPlane(Vector4 v, Vector4 plane, Vector4 normal) {
    int dotProduct = Vector::DotProduct(normal, plane);
    int result = 0;

    result += ((Sint64)v.X * normal.X) >> 16;
    result += ((Sint64)v.Y * normal.Y) >> 16;
    result += ((Sint64)v.Z * normal.Z) >> 16;
    result += ((Sint64)v.W * normal.W) >> 16;
    result -= dotProduct;

    return result;
}
