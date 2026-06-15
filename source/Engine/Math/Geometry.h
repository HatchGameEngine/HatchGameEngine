#ifndef ENGINE_MATH_GEOMETRY_H
#define ENGINE_MATH_GEOMETRY_H

#include <Engine/Includes/Standard.h>
#include <Engine/Math/GeometryTypes.h>

class Geometry {
public:
	static vector<Polygon2D>* Triangulate(Polygon2D& input, vector<Polygon2D> holes);
	static vector<Polygon2D>* Intersect(unsigned clipType,
		unsigned fillRule,
		vector<Polygon2D> inputSubjects,
		vector<Polygon2D> inputClips);
};

#endif /* ENGINE_MATH_GEOMETRY_H */
