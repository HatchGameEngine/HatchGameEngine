#ifndef ENGINE_DIAGNOSTICS_CLOCK_H
#define ENGINE_DIAGNOSTICS_CLOCK_H

#include <Engine/Includes/Standard.h>

namespace Clock {
//public:
	void Init();
	void Start();
	double GetTicks();
	double End();
	void Delay(double milliseconds);
};

#endif /* ENGINE_DIAGNOSTICS_CLOCK_H */
