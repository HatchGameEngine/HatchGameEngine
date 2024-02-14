#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Math/GeometryTypes.h>

class Geometry {
public:
};
#endif

#include <Engine/Math/Geometry.h>
#include <Engine/Math/Math.h>

#include <Libraries/Clipper2/clipper.h>

PRIVATE STATIC bool Geometry::CheckEar(vector<FVector2>& input, unsigned count, unsigned prev, unsigned curr, unsigned next) {
    FVector2& a = input[prev];
    FVector2& b = input[curr];
    FVector2& c = input[next];

    FVector2 ab = b - a;
    FVector2 bc = c - b;
    FVector2 ca = a - c;

    if (FVector2::Determinant(ab, bc) < 0)
        return false;

    float da = FVector2::Determinant(ab, a);
    float db = FVector2::Determinant(bc, b);
    float dc = FVector2::Determinant(ca, c);

    unsigned i = (next + 1) % count;
    while (i != prev) {
        FVector2& point = input[i];

        if (FVector2::Determinant(ab, point) > da
        && FVector2::Determinant(bc, point) > db
        && FVector2::Determinant(ca, point) > dc)
            return false;

        i = (i + 1) % count;
    }

    return true;
}

PRIVATE STATIC int Geometry::GetPointForTriangulation(int point, unsigned count) {
    if (point < 0)
        return point + count;
    else if (point >= count)
        return point % count;
    else
        return point;
}

PUBLIC STATIC vector<Polygon2D>* Geometry::Triangulate(Polygon2D& input) {
    vector<FVector2> points = input.Points;

    unsigned count = points.size();
    if (!count || count < 3)
        return nullptr;

    vector<Polygon2D>* output = new vector<Polygon2D>();

    int winding = input.CalculateWinding();

    while (count > 3) {
        unsigned curr = 0;
        unsigned prev, next;

        while (curr < count) {
            prev = GetPointForTriangulation(curr - winding, count);
            next = GetPointForTriangulation(curr + winding, count);
            if (CheckEar(points, count, next, curr, prev))
                break;
            curr++;
        }

        Triangle tri(points[next], points[prev], points[GetPointForTriangulation(curr, count)]);
        output->push_back(tri.ToPolygon());
        points.erase(points.begin() + curr);
        count--;
    }

    if (winding < 0) {
        Triangle tri(points[2], points[1], points[0]);
        output->push_back(tri.ToPolygon());
    }
    else {
        Triangle tri(points);
        output->push_back(tri.ToPolygon());
    }

    return output;
}

static Clipper2Lib::ClipType GetClipType(unsigned clipType) {
    switch (clipType) {
    case GeoBooleanOp_Intersection:
        return Clipper2Lib::ClipType::Intersection;
    case GeoBooleanOp_Union:
        return Clipper2Lib::ClipType::Union;
    case GeoBooleanOp_Difference:
        return Clipper2Lib::ClipType::Difference;
    case GeoBooleanOp_ExclusiveOr:
        return Clipper2Lib::ClipType::Xor;
    default:
        return Clipper2Lib::ClipType::None;
    }
}

static Clipper2Lib::FillRule GetFillRule(unsigned fillRule) {
    switch (fillRule) {
    case GeoFillRule_EvenOdd:
        return Clipper2Lib::FillRule::EvenOdd;
    case GeoFillRule_NonZero:
        return Clipper2Lib::FillRule::NonZero;
    case GeoFillRule_Positive:
        return Clipper2Lib::FillRule::Positive;
    case GeoFillRule_Negative:
        return Clipper2Lib::FillRule::Negative;
    default:
        return Clipper2Lib::FillRule::EvenOdd;
    }
}

PUBLIC STATIC vector<Polygon2D>* Geometry::Intersect(unsigned clipType, unsigned fillRule, vector<Polygon2D> inputSubjects, vector<Polygon2D> inputClips) {
    Clipper2Lib::PathsD subjects;
    Clipper2Lib::PathsD clips;

    for (Polygon2D& sub : inputSubjects)
        subjects.push_back(Clipper2Lib::MakePathD(sub.ToList()));
    for (Polygon2D& clip : inputClips)
        clips.push_back(Clipper2Lib::MakePathD(clip.ToList()));

    Clipper2Lib::ClipperD clipper;
    clipper.AddSubject(subjects);
    clipper.AddClip(clips);

    Clipper2Lib::PathsD result;
    clipper.Execute(GetClipType(clipType), GetFillRule(fillRule), result);

    vector<Polygon2D>* output = new vector<Polygon2D>();

    for (Clipper2Lib::PathD& path : result) {
        Polygon2D polygon;

        for (Clipper2Lib::PointD& point : path) {
            FVector2 vec(point.x, point.y);
            polygon.Points.push_back(vec);
        }

        output->push_back(polygon);
    }

    return output;
}
