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
    static int     DotProduct(Vector4 v1, Vector4 v2);
    static int     Length(Vector4 v);
    static Vector4 Normalize(Vector4 v);
    static int     IntersectWithPlane(Vector4 plane, Vector4 normal, Vector4 v1, Vector4 v2);
    static int     DistanceToPlane(Vector4 v, Vector4 plane, Vector4 normal);
};

#endif /* ENGINE_MATH_VECTOR_H */
