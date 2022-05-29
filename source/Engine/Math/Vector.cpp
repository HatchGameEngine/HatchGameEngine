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
    result.X = (v.X * t) / 0x10000;
    result.Y = (v.Y * t) / 0x10000;
    result.Z = (v.Z * t) / 0x10000;
    result.W = (v.W * t) / 0x10000;
    return result;
}

PUBLIC STATIC Sint64  Vector::DotProduct(Vector4 v1, Vector4 v2) {
    Sint64 result = 0;
    result += (v1.X * v2.X) / 0x10000;
    result += (v1.Y * v2.Y) / 0x10000;
    result += (v1.Z * v2.Z) / 0x10000;
    result += (v1.W * v2.W) / 0x10000;
    return result;
}

PUBLIC STATIC Sint64  Vector::Length(Vector4 v) {
    float result = Vector::DotProduct(v, v);
    result = sqrtf(result / 0x10000);
    return result * 0x10000;
}

PUBLIC STATIC Vector4 Vector::Normalize(Vector4 v) {
    Vector4 result;

    Sint64 length = Vector::Length(v);
    if (length == 0)
        result.X = result.Y = result.Z = result.W = 0;
    else {
        result.X = (v.X * 0x10000) / length;
        result.Y = (v.Y * 0x10000) / length;
        result.Z = (v.Z * 0x10000) / length;
        result.W = (v.W * 0x10000) / length;
    }

    return result;
}

PUBLIC STATIC Sint64  Vector::IntersectWithPlane(Vector4 plane, Vector4 normal, Vector4 v1, Vector4 v2) {
    Vector4 planeNormal = Vector::Normalize(normal);
    Sint64 dotProduct = Vector::DotProduct(v1, planeNormal);
    Sint64 t = Vector::DotProduct(planeNormal, plane) - dotProduct;
    Sint64 diff = Vector::DotProduct(v2, planeNormal) - dotProduct;
    if (diff == 0)
        return 0;

    return (t * 0x10000) / diff;
}

PUBLIC STATIC Sint64  Vector::DistanceToPlane(Vector4 v, Vector4 plane, Vector4 normal) {
    Sint64 dotProduct = Vector::DotProduct(normal, plane);
    Sint64 result = 0;

    result += (v.X * normal.X) / 0x10000;
    result += (v.Y * normal.Y) / 0x10000;
    result += (v.Z * normal.Z) / 0x10000;
    result += (v.W * normal.W) / 0x10000;
    result -= dotProduct;

    return result;
}
