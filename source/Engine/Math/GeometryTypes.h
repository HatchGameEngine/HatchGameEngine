#ifndef GEOMETRYTYPES_H
#define GEOMETRYTYPES_H

#include <Engine/Math/VectorTypes.h>
#include <Engine/Math/Math.h>

enum {
    GeoBooleanOp_Intersection,
    GeoBooleanOp_Union,
    GeoBooleanOp_Difference,
    GeoBooleanOp_ExclusiveOr
};

enum {
    GeoFillRule_EvenOdd,
    GeoFillRule_NonZero,
    GeoFillRule_Positive,
    GeoFillRule_Negative
};

struct Polygon2D {
    vector<FVector2> Points;

    Polygon2D() {
        Points.resize(0);
    }

    Polygon2D(vector<FVector2> input) {
        Points = input;
    }

    vector<double> ToList() {
        unsigned count = Points.size();

        vector<double> vertices;

        for (unsigned i = 0; i < count; i++) {
            vertices.push_back(Points[i].X);
            vertices.push_back(Points[i].Y);
        }

        return vertices;
    }

    int CalculateWinding() {
        if (Points.size() == 0)
            return 0;

        float calc = 0.0;

        size_t numPoints = Points.size();

        FVector2& prev = Points[numPoints - 1];
        for (size_t i = 0; i < numPoints; i++) {
            FVector2& next = Points[i];
            calc += (next.X - prev.X) * (next.Y + prev.Y);
            prev = next;
        }

        if (calc > 0.0)
            return 1;

        return -1;
    }
};

struct Triangle {
    FVector2 A, B, C;

    Triangle(FVector2 a, FVector2 b, FVector2 c) {
        A = a;
        B = b;
        C = c;
    }

    Triangle(vector<FVector2>& points) {
        A = points[0];
        B = points[1];
        C = points[2];
    }

    Polygon2D ToPolygon() {
        Polygon2D tri;
        tri.Points.push_back(A);
        tri.Points.push_back(B);
        tri.Points.push_back(C);
        return tri;
    }

    bool IsClockwise() {
        return FVector2::CrossProduct(A, B, C) < 0;
    }

    bool IsCounterClockwise() {
        return FVector2::CrossProduct(A, B, C) > 0;
    }

    float GetArea() {
        float area =
            A.X * (B.Y - C.Y) +
            B.X * (C.Y - A.Y) +
            C.X * (A.Y - B.Y);

        return area / 2;
    }

    bool IsPointInside(FVector2 point) {
        FVector2 v1 = C - A;
        FVector2 v2 = B - A;
        FVector2 v3 = point - A;

        float d11 = FVector2::DotProduct(v1, v1);
        float d12 = FVector2::DotProduct(v1, v2);
        float d13 = FVector2::DotProduct(v1, v3);
        float d22 = FVector2::DotProduct(v2, v2);
        float d23 = FVector2::DotProduct(v2, v3);

        float denom = (d11 * d22) - (d12 * d12);
        if (fabsf(denom) <= 0.0)
            return true;

        float invDenom = 1.0 / denom;
        float u = (d22 * d13 - d12 * d23) * invDenom;
        float v = (d11 * d23 - d12 * d13) * invDenom;

        if (u >= 0.0 && v >= 0.0 && (u + v) < 1.0)
            return true;

        return false;
    }
};

#endif /* GEOMETRYTYPES_H */
