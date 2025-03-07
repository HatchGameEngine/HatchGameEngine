#ifndef ENGINE_DIAGNOSTICS_CLOCK_H
#define ENGINE_DIAGNOSTICS_CLOCK_H

#include <Engine/Includes/Standard.h>

class Clock {
public:
	static void Init();
	static void Start();
	static double GetTicks();
	static double End();
	static void Delay(double milliseconds);
};

#endif /* ENGINE_DIAGNOSTICS_CLOCK_H */
