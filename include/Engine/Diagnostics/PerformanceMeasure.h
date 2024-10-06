#ifndef ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H
#define ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H

#include <Engine/Includes/Standard.h>
#include <Engine/Diagnostics/PerformanceTypes.h>

class PerformanceMeasure {
public:
    static bool            Initialized;
    static Perf_ViewRender PERF_ViewRender[MAX_SCENE_VIEWS];

    static void Init();
};

#endif /* ENGINE_DIAGNOSTICS_PERFORMANCEMEASURE_H */
