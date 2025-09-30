#ifndef ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H
#define ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H

#include <Engine/Includes/Standard.h>

class PerformanceMeasure {
private:
	double StartTime;
	double EndTime;

public:
	const char* Name;
	double Time;
	struct {
		float R, G, B;
	} Colors;

	PerformanceMeasure() {};

	PerformanceMeasure(const char* name, float r, float g, float b) {
		Name = name;
		Colors.R = r;
		Colors.G = g;
		Colors.B = b;
	};

	PerformanceMeasure(const char* name) {
		Name = name;
		Colors.R = 0.0f;
		Colors.G = 1.0f;
		Colors.B = 0.0f;
	};

	void Reset();
	void Begin();
	void End();
	void Accumulate();
};

#endif /* ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H */
