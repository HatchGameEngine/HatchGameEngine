#if INTERFACE
#include <Engine/Includes/Standard.h>
class Math {
public:
};
#endif

int Sin1024LookupTable[0x400];
int Cos1024LookupTable[0x400];
int Tan1024LookupTable[0x400];
int ASin1024LookupTable[0x400];
int ACos1024LookupTable[0x400];

int Sin512LookupTable[0x200];
int Cos512LookupTable[0x200];
int Tan512LookupTable[0x200];
int ASin512LookupTable[0x200];
int ACos512LookupTable[0x200];

int Sin256LookupTable[0x100];
int Cos256LookupTable[0x100];
int Tan256LookupTable[0x100];
int ASin256LookupTable[0x100];
int ACos256LookupTable[0x100];

int ArcTan256LookupTable[0x100 * 0x100];

int randSeed = 0;

#include <Engine/Math/Math.h>
#include <time.h>

PUBLIC STATIC void Math::Init() {
    srand((Uint32)time(NULL));

    Math::ClearTrigLookupTables();
    Math::CalculateTrigAngles();
}
// Trig functions
PUBLIC STATIC float Math::Cos(float n) {
    return std::cos(n);
}
PUBLIC STATIC float Math::Sin(float n) {
    return std::sin(n);
}
PUBLIC STATIC float Math::Tan(float n) {
    return std::tan(n);
}
PUBLIC STATIC float Math::Asin(float x) {
    return std::asin(x);
}
PUBLIC STATIC float Math::Acos(float x) {
    return std::acos(x);
}
PUBLIC STATIC float Math::Atan(float x, float y) {
    if (x == 0.0f && y == 0.0f) return 0.0f;

    return std::atan2(y, x);
}
PUBLIC STATIC float Math::Distance(float x1, float y1, float x2, float y2) {
    x2 -= x1; x2 *= x2;
    y2 -= y1; y2 *= y2;
    return (float)sqrt(x2 + y2);
}
PUBLIC STATIC float Math::Hypot(float a, float b, float c) {
    return (float)sqrt(a * a + b * b + c * c);
}
PUBLIC STATIC void Math::ClearTrigLookupTables() {
    memset(Sin256LookupTable, 0, sizeof(Sin256LookupTable));
    memset(Cos256LookupTable, 0, sizeof(Cos256LookupTable));
    memset(Tan256LookupTable, 0, sizeof(Tan256LookupTable));
    memset(ASin256LookupTable, 0, sizeof(ASin256LookupTable));
    memset(ACos256LookupTable, 0, sizeof(ACos256LookupTable));
    memset(Sin512LookupTable, 0, sizeof(Sin512LookupTable));
    memset(Cos512LookupTable, 0, sizeof(Cos512LookupTable));
    memset(Tan512LookupTable, 0, sizeof(Tan512LookupTable));
    memset(ASin512LookupTable, 0, sizeof(ASin512LookupTable));
    memset(ACos512LookupTable, 0, sizeof(ACos512LookupTable));
    memset(Sin1024LookupTable, 0, sizeof(Sin1024LookupTable));
    memset(Cos1024LookupTable, 0, sizeof(Cos1024LookupTable));
    memset(Tan1024LookupTable, 0, sizeof(Tan1024LookupTable));
    memset(ASin1024LookupTable, 0, sizeof(ASin1024LookupTable));
    memset(ACos1024LookupTable, 0, sizeof(ACos1024LookupTable));
    memset(ArcTan256LookupTable, 0, sizeof(ArcTan256LookupTable));
    randSeed = 0;
}
PUBLIC STATIC void Math::CalculateTrigAngles() {
    randSeed = rand();

    for (int i = 0; i < 0x400; i++) {
        Sin1024LookupTable[i] = (int)(sinf((i / 512.0) * R_PI) * 1024.0);
        Cos1024LookupTable[i] = (int)(cosf((i / 512.0) * R_PI) * 1024.0);
        Tan1024LookupTable[i] = (int)(tanf((i / 512.0) * R_PI) * 1024.0);
        ASin1024LookupTable[i] = (int)((asinf(i / 1023.0) * 512.0) / R_PI);
        ACos1024LookupTable[i] = (int)((acosf(i / 1023.0) * 512.0) / R_PI);
    }

    Cos1024LookupTable[0x000] = 0x400;
    Cos1024LookupTable[0x100] = 0;
    Cos1024LookupTable[0x200] = -0x400;
    Cos1024LookupTable[0x300] = 0;

    Sin1024LookupTable[0x000] = 0;
    Sin1024LookupTable[0x100] = 0x400;
    Sin1024LookupTable[0x200] = 0;
    Sin1024LookupTable[0x300] = -0x400;

    for (int i = 0; i < 0x200; i++) {
        Sin512LookupTable[i] = (int)(sinf((i / 256.0) * R_PI) * 512.0);
        Cos512LookupTable[i] = (int)(cosf((i / 256.0) * R_PI) * 512.0);
        Tan512LookupTable[i] = (int)(tanf((i / 256.0) * R_PI) * 512.0);
        ASin512LookupTable[i] = (int)((asinf(i / 511.0) * 256.0) / R_PI);
        ACos512LookupTable[i] = (int)((acosf(i / 511.0) * 256.0) / R_PI);
    }

    Cos512LookupTable[0x00] = 0x200;
    Cos512LookupTable[0x80] = 0;
    Cos512LookupTable[0x100] = -0x200;
    Cos512LookupTable[0x180] = 0;

    Sin512LookupTable[0x00] = 0;
    Sin512LookupTable[0x80] = 0x200;
    Sin512LookupTable[0x100] = 0;
    Sin512LookupTable[0x180] = -0x200;

    for (int i = 0; i < 0x100; i++) {
        Sin256LookupTable[i] = (int)((Sin512LookupTable[i * 2] >> 1));
        Cos256LookupTable[i] = (int)((Cos512LookupTable[i * 2] >> 1));
        Tan256LookupTable[i] = (int)((Tan512LookupTable[i * 2] >> 1));
        ASin256LookupTable[i] = (int)((asinf(i / 255.0) * 128.0) / R_PI);
        ACos256LookupTable[i] = (int)((acosf(i / 255.0) * 128.0) / R_PI);
    }
}
PUBLIC STATIC int Math::Sin1024(int angle) {
    return Sin1024LookupTable[angle & 0x3FF];
}
PUBLIC STATIC int Math::Cos1024(int angle) {
    return Cos1024LookupTable[angle & 0x3FF];
}
PUBLIC STATIC int Math::Tan1024(int angle) {
    return Tan1024LookupTable[angle & 0x3FF];
}
PUBLIC STATIC int Math::ASin1024(int angle) {
    if (angle > 0x3FF)
        return 0;
    if (angle < 0)
        return -ASin1024LookupTable[-angle];
    return ASin1024LookupTable[angle];
}
PUBLIC STATIC int Math::ACos1024(int angle) {
    if (angle > 0x3FF)
        return 0;
    if (angle < 0)
        return -ACos1024LookupTable[-angle];
    return ACos1024LookupTable[angle];
}
PUBLIC STATIC int Math::Sin512(int angle) {
    return Sin512LookupTable[angle & 0x1FF];
}
PUBLIC STATIC int Math::Cos512(int angle) {
    return Cos512LookupTable[angle & 0x1FF];
}
PUBLIC STATIC int Math::Tan512(int angle) {
    return Tan512LookupTable[angle & 0x1FF];
}
PUBLIC STATIC int Math::ASin512(int angle) {
    if (angle > 0x1FF)
        return 0;
    if (angle < 0)
        return -ASin512LookupTable[-angle];
    return ASin512LookupTable[angle];
}
PUBLIC STATIC int Math::ACos512(int angle) {
    if (angle > 0x1FF)
        return 0;
    if (angle < 0)
        return -ACos512LookupTable[-angle];
    return ACos512LookupTable[angle];
}
PUBLIC STATIC int Math::Sin256(int angle) {
    return Sin256LookupTable[angle & 0XFF];
}
PUBLIC STATIC int Math::Cos256(int angle) {
    return Cos256LookupTable[angle & 0XFF];
}
PUBLIC STATIC int Math::Tan256(int angle) {
    return Tan256LookupTable[angle & 0xFF];
}
PUBLIC STATIC int Math::ASin256(int angle) {
    if (angle > 0xFF)
        return 0;
    if (angle < 0)
        return -ASin256LookupTable[-angle];
    return ASin256LookupTable[angle];
}
PUBLIC STATIC int Math::ACos256(int angle) {
    if (angle > 0XFF)
        return 0;
    if (angle < 0)
        return -ACos256LookupTable[-angle];
    return ACos256LookupTable[angle];
}

// help
PUBLIC STATIC int   Math::CeilPOT(int n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

// Deterministic functions
PUBLIC STATIC float Math::Abs(float n) {
    return std::abs(n);
}
PUBLIC STATIC float Math::Max(float a, float b) {
    return a > b ? a : b;
}
PUBLIC STATIC float Math::Min(float a, float b) {
    return a < b ? a : b;
}
PUBLIC STATIC float Math::Clamp(float v, float a, float b) {
    return Math::Max(a, Math::Min(v, b));
}
PUBLIC STATIC float Math::Sign(float a) {
    return a < 0.0f ? -1.0f : a > 0.0f ? 1.0f : 0.0f;
}

// Random functions (non-inclusive)
PUBLIC STATIC float Math::Random() {
    return rand() / (float)RAND_MAX;
}
PUBLIC STATIC float Math::RandomMax(float max) {
    return Math::Random() * max;
}
PUBLIC STATIC float Math::RandomRange(float min, float max) {
    return (Math::Random() * (max - min)) + min;
}
PUBLIC STATIC int Math::GetRandSeed() {
    return randSeed;
}
PUBLIC STATIC void Math::SetRandSeed(int key) {
    randSeed = key;
}
PUBLIC STATIC int Math::RandomInteger(int min, int max) {
    int seed1   = 1103515245 * randSeed + 12345;
    int seed2   = 1103515245 * seed1 + 12345;
    randSeed    = 1103515245 * seed2 + 12345;

    int result  = ((randSeed >> 16) & 0x7FF) ^ ((((seed1 >> 6) & 0x1FFC00) ^ ((seed2 >> 16) & 0x7FF)) << 10);
    int size    = abs(max - min);

    if (min > max)
        return (result - result / size * size + max);
    else if (min < max)
        return (result - result / size * size + min);
    else
        return max;
}
PUBLIC STATIC int Math::RandomIntegerSeeded(int min, int max, int seed) {
    if (!randSeed)
        return 0;

    int seed1   = 1103515245 * randSeed + 12345;
    int seed2   = 1103515245 * seed1 + 12345;
    randSeed   = 1103515245 * seed2 + 12345;

    int result  = ((randSeed >> 16) & 0x7FF) ^ ((((seed1 >> 6) & 0x1FFC00) ^ ((seed2 >> 16) & 0x7FF)) << 10);
    int size    = abs(max - min);

    if (min > max)
        return (result - result / size * size + max);
    else if (min < max)
        return (result - result / size * size + min);
    else
        return max;
}
//
