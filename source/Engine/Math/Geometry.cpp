#include <Engine/Math/Geometry.h>
#include <Engine/Math/Math.h>

#include <Engine/Diagnostics/Log.h>

#include <Libraries/Clipper2/clipper.h>
#include <Libraries/poly2tri/poly2tri.h>

bool Geometry::CheckEar(vector<FVector2>& input,
	unsigned count,
	unsigned prev,
	unsigned curr,
	unsigned next) {
	FVector2& a = input[prev];
	FVector2& b = input[curr];
	FVector2& c = input[next];

	FVector2 ab = b - a;
	FVector2 bc = c - b;
	FVector2 ca = a - c;

	if (FVector2::Determinant(ab, bc) < 0) {
		return false;
	}

	float da = FVector2::Determinant(ab, a);
	float db = FVector2::Determinant(bc, b);
	float dc = FVector2::Determinant(ca, c);

	unsigned i = (next + 1) % count;
	while (i != prev) {
		FVector2& point = input[i];

		if (FVector2::Determinant(ab, point) > da &&
			FVector2::Determinant(bc, point) > db &&
			FVector2::Determinant(ca, point) > dc) {
			return false;
		}

		i = (i + 1) % count;
	}

	return true;
}

int Geometry::GetPointForTriangulation(int point, unsigned count) {
	if (point < 0) {
		return point + count;
	}
	else if (point >= count) {
		return point % count;
	}
	else {
		return point;
	}
}

static std::vector<p2t::Point*> GetP2TPoints(Polygon2D& input) {
	std::vector<p2t::Point*> points;

	for (unsigned i = 0; i < input.Points.size(); i++) {
		points.push_back(new p2t::Point(input.Points[i].X, input.Points[i].Y));
	}

	return points;
}

static void FreeP2TPoints(std::vector<p2t::Point*>& points) {
	for (unsigned i = 0; i < points.size(); i++) {
		delete points[i];
	}
}

vector<Polygon2D>* Geometry::Triangulate(Polygon2D& input, vector<Polygon2D> holes) {
	vector<FVector2> points = input.Points;

	unsigned count = points.size();
	if (!count || count < 3) {
		return nullptr;
	}

	vector<Polygon2D>* output = new vector<Polygon2D>();

	std::vector<p2t::Point*> inputPoly = GetP2TPoints(input);
	std::vector<std::vector<p2t::Point*>> inputHoles;

	p2t::CDT* cdt = new p2t::CDT(inputPoly);

	for (Polygon2D hole : holes) {
		std::vector<p2t::Point*> inputHole = GetP2TPoints(hole);
		inputHoles.push_back(inputHole);
		cdt->AddHole(inputHole);
	}

	try {
		cdt->Triangulate();

		vector<p2t::Triangle*> triangles = cdt->GetTriangles();

		for (unsigned int i = 0; i < triangles.size(); i++) {
			p2t::Triangle& t = *triangles[i];
			p2t::Point& a = *t.GetPoint(0);
			p2t::Point& b = *t.GetPoint(1);
			p2t::Point& c = *t.GetPoint(2);

			Polygon2D polygon;
			polygon.AddPoint(a.x, a.y);
			polygon.AddPoint(b.x, b.y);
			polygon.AddPoint(c.x, c.y);

			output->push_back(polygon);
		}
	} catch (std::runtime_error& err) {
		Log::Print(Log::LOG_ERROR, "Geometry::Triangulate error: %s", err.what());
	}

	FreeP2TPoints(inputPoly);

	for (std::vector<p2t::Point*> hole : inputHoles) {
		FreeP2TPoints(hole);
	}

	delete cdt;

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

vector<Polygon2D>* Geometry::Intersect(unsigned clipType,
	unsigned fillRule,
	vector<Polygon2D> inputSubjects,
	vector<Polygon2D> inputClips) {
	Clipper2Lib::PathsD subjects;
	Clipper2Lib::PathsD clips;

	for (Polygon2D& sub : inputSubjects) {
		subjects.push_back(Clipper2Lib::MakePathD(sub.ToList()));
	}
	for (Polygon2D& clip : inputClips) {
		clips.push_back(Clipper2Lib::MakePathD(clip.ToList()));
	}

	Clipper2Lib::ClipperD clipper;
	clipper.AddSubject(subjects);
	clipper.AddClip(clips);

	Clipper2Lib::PathsD result;
	clipper.Execute(GetClipType(clipType), GetFillRule(fillRule), result);

	vector<Polygon2D>* output = new vector<Polygon2D>();

	for (Clipper2Lib::PathD& path : result) {
		Polygon2D polygon;

		for (Clipper2Lib::PointD& point : path) {
			polygon.AddPoint(point.x, point.y);
		}

		output->push_back(polygon);
	}

	return output;
}
