#ifndef ENGINE_MATH_RANDOM_H
#define ENGINE_MATH_RANDOM_H

#include <Engine/Includes/Standard.h>

class Random {
public:
	static Sint32 Seed;

	static void SetSeed(Sint32 seed);
	static float Get();
	static float Max(float max);
	static float Range(float min, float max);
};

#endif /* ENGINE_MATH_RANDOM_H */
