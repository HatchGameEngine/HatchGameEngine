#ifndef ENGINE_DIAGNOSTICS_PERFORMANCETYPES
#define ENGINE_DIAGNOSTICS_PERFORMANCETYPES

#include <Engine/Diagnostics/PerformanceMeasure.h>

struct ApplicationMetrics {
	PerformanceMeasure Event;
	PerformanceMeasure AfterScene;
	PerformanceMeasure Poll;
	PerformanceMeasure Update;
	PerformanceMeasure Clear;
	PerformanceMeasure Render;
	PerformanceMeasure PostProcess;
	PerformanceMeasure FPSCounter;
	PerformanceMeasure Present;
	PerformanceMeasure Frame;
};

struct Perf_ViewRender {
	double RenderSetupTime;
	bool RecreatedDrawTarget;
	double ProjectionSetupTime;
	double ObjectRenderEarlyTime;
	double ObjectRenderTime;
	double ObjectRenderLateTime;
	double LayerTileRenderTime[32]; // MAX_LAYERS
	double RenderFinishTime;
	double RenderTime;
};

#endif /* ENGINE_DIAGNOSTICS_PERFORMANCETYPES */
