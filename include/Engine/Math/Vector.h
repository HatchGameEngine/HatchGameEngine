#ifndef ENGINE_MATH_VECTOR_H
#define ENGINE_MATH_VECTOR_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>

class Vector {
public:
    static Vector4 Add(Vector4 v1, Vector4 v2);
    static Vector4 Subtract(Vector4 v1, Vector4 v2);
    static Vector4 Multiply(Vector4 v, int t);
    static Vector3 Multiply(Vector3 v, Matrix4x4* m);
    static Vector3 Interpolate(Vector3 v1, Vector3 v2, Sint64 t);
    static Vector4 Interpolate(Vector4 v1, Vector4 v2, Sint64 t);
    static Sint64  DotProduct(Vector4 v1, Vector4 v2);
    static Sint64  Length(Vector4 v);
    static Vector4 Normalize(Vector4 v);
    static Sint64  IntersectWithPlane(Vector4 plane, Vector4 normal, Vector4 v1, Vector4 v2);
    static Sint64  DistanceToPlane(Vector4 v, Vector4 plane, Vector4 normal);
};

#endif /* ENGINE_MATH_VECTOR_H */
