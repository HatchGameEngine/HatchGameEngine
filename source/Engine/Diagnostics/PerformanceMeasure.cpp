#include <Engine/Diagnostics/PerformanceMeasure.h>

bool PerformanceMeasure::Initialized = false;
Perf_ViewRender PerformanceMeasure::PERF_ViewRender[MAX_SCENE_VIEWS];

void PerformanceMeasure::Init() {
	if (PerformanceMeasure::Initialized) {
		return;
	}

	PerformanceMeasure::Initialized = true;
	memset(PerformanceMeasure::PERF_ViewRender, 0, sizeof(PerformanceMeasure::PERF_ViewRender));
}
