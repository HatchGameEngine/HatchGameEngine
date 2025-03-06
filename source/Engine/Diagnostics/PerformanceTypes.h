#ifndef ENGINE_DIAGNOSTICS_PERFORMANCETYPES
#define ENGINE_DIAGNOSTICS_PERFORMANCETYPES

struct Perf_Application {
	double EventTime;
	double AfterSceneTime;
	double PollTime;
	double UpdateTime;
	double ClearTime;
	double RenderTime;
	double FPSCounterTime;
	double PresentTime;
	double FrameTime;
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
