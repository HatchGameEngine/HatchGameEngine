#ifndef ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H
#define ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H

#include <Engine/Diagnostics/PerformanceTypes.h>
#include <Engine/Includes/Standard.h>

namespace PerformanceMeasure {
//public:
	extern bool Initialized;
	extern Perf_ViewRender PERF_ViewRender[MAX_SCENE_VIEWS];

	void Init();
};

#endif /* ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H */
