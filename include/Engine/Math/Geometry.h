#ifndef ENGINE_MATH_GEOMETRY_H
#define ENGINE_MATH_GEOMETRY_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Math/GeometryTypes.h>

class Geometry {
private:
    static bool CheckEar(vector<FVector2>& input, unsigned count, unsigned prev, unsigned curr, unsigned next);
    static int GetPointForTriangulation(int point, unsigned count);

public:
    static vector<Polygon2D>* Triangulate(Polygon2D& input, vector<Polygon2D> holes);
    static vector<Polygon2D>* Intersect(unsigned clipType, unsigned fillRule, vector<Polygon2D> inputSubjects, vector<Polygon2D> inputClips);
};

#endif /* ENGINE_MATH_GEOMETRY_H */
