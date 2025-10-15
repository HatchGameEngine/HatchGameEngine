#ifndef ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H
#define ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H

#include <Engine/Includes/Standard.h>

class PerformanceMeasure {
private:
	double StartTime;
	double EndTime;
	bool* Active = nullptr;

public:
	const char* Name;
	double Time;
	struct {
		float R, G, B;
	} Colors;

	PerformanceMeasure() {};
	PerformanceMeasure(const char*, float, float, float);
	PerformanceMeasure(const char*, float, float, float, bool*);
	PerformanceMeasure(const char*);

	bool IsActive();

	void Reset();
	void Begin();
	void End();
	void Accumulate();
};

#endif /* ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H */
