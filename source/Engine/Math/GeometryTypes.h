#ifndef GEOMETRYTYPES_H
#define GEOMETRYTYPES_H

#include <Engine/Math/VectorTypes.h>
#include <Engine/Math/Math.h>

struct Triangle {
    FVector2 A, B, C;

    Triangle(FVector2 a, FVector2 b, FVector2 c) {
        A = a;
        B = b;
        C = c;
    }

    static float StaticGetArea(FVector2 a, FVector2 b, FVector2 c) {
        float area =
            a.X * (b.Y - c.Y) +
            b.X * (c.Y - a.Y) +
            c.X * (a.Y - b.Y);

        return area / 2;
    }

    static float StaticGetAngle(FVector2 a, FVector2 b, FVector2 c) {
        float angle = std::atan2((c.Y - b.Y), (c.X - b.X)) - std::atan2((a.Y - b.Y), (a.X - b.X));
        if (angle < 0.0)
            return (M_PI * 2.0) + angle;
        return angle;
    }

    static bool StaticIsPointInside(FVector2 a, FVector2 b, FVector2 c, FVector2 point) {
        float aa = Triangle::StaticGetArea(a, b, point);
        float ab = Triangle::StaticGetArea(b, c, point);
        float ac = Triangle::StaticGetArea(c, a, point);

        float signA = Math::Sign(aa);
        float signB = Math::Sign(ab);
        float signC = Math::Sign(ac);

        if (Math::Sign(aa) != Math::Sign(ab))
            return false;

        return Math::Sign(aa) == Math::Sign(ac);
    }

    float GetArea() {
        return Triangle::StaticGetArea(A, B, C);
    }

    float GetAngle() {
        return Triangle::StaticGetAngle(A, B, C);
    }

    bool IsPointInside(FVector2 point) {
        return Triangle::StaticIsPointInside(A, B, C, point);
    }
};

#endif /* GEOMETRYTYPES_H */
