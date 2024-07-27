#if INTERFACE
#include <Engine/Includes/Standard.h>
class Random {
public:
    static Sint32 Seed;
};
#endif

#include <Engine/Math/Random.h>

#define INITIAL_RANDOM_SEED 1938465012

Sint32 Random::Seed = INITIAL_RANDOM_SEED;

PUBLIC STATIC void Random::SetSeed(Sint32 seed) {
    if (!seed)
        seed = INITIAL_RANDOM_SEED;

    Seed = seed;
}

PUBLIC STATIC float Random::Get() {
    Sint32 nextSeed = (1103515245 * Seed + 12345) & 0x7FFFFFFF;
    Seed = nextSeed;
    return ((float)nextSeed) / 0x7FFFFFFF;
}

PUBLIC STATIC float Random::Max(float max) {
    return Random::Get() * max;
}

PUBLIC STATIC float Random::Range(float min, float max) {
    return (Random::Get() * (max - min)) + min;
}
