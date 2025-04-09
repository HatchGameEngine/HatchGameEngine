#ifndef ENGINE_MATH_RANDOM_H
#define ENGINE_MATH_RANDOM_H

#include <Engine/Includes/Standard.h>

namespace Random {
//public:
	extern Sint32 Seed;

	void SetSeed(Sint32 seed);
	float Get();
	float Max(float max);
	float Range(float min, float max);
};

#endif /* ENGINE_MATH_RANDOM_H */
