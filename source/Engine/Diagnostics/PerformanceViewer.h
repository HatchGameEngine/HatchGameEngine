#ifndef ENGINE_DIAGNOSTICS_PERFORMANCEVIEWER_H
#define ENGINE_DIAGNOSTICS_PERFORMANCEVIEWER_H

#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/Font.h>

class PerformanceViewer {
public:
	static void DrawFramerate(Font* font);
	static void DrawDetailed(Font* font);
};

#endif /* ENGINE_DIAGNOSTICS_PERFORMANCEVIEWER_H */
