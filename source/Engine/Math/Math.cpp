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

unsigned int ArcTan256LookupTable[0x100 * 0x100];

int randSeed = 0;

#include <Engine/Math/Math.h>
#include <time.h>

void Math::Init() {
	srand((Uint32)time(NULL));

	Math::ClearTrigLookupTables();
	Math::CalculateTrigAngles();
}
// Trig functions
float Math::Cos(float n) {
	return std::cos(n);
}
float Math::Sin(float n) {
	return std::sin(n);
}
float Math::Tan(float n) {
	return std::tan(n);
}
float Math::Asin(float x) {
	return std::asin(x);
}
float Math::Acos(float x) {
	return std::acos(x);
}
float Math::Atan(float x, float y) {
	if (x == 0.0f && y == 0.0f) {
		return 0.0f;
	}

	return std::atan2(y, x);
}
float Math::Distance(float x1, float y1, float x2, float y2) {
	x2 -= x1;
	x2 *= x2;
	y2 -= y1;
	y2 *= y2;
	return (float)sqrt(x2 + y2);
}
float Math::Hypot(float a, float b, float c) {
	return (float)sqrt(a * a + b * b + c * c);
}
void Math::ClearTrigLookupTables() {
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
void Math::CalculateTrigAngles() {
	randSeed = rand();

	for (int i = 0; i < 0x400; i++) {
		Sin1024LookupTable[i] = (int)(sinf((i / 512.0) * RSDK_PI) * 1024.0);
		Cos1024LookupTable[i] = (int)(cosf((i / 512.0) * RSDK_PI) * 1024.0);
		Tan1024LookupTable[i] = (int)(tanf((i / 512.0) * RSDK_PI) * 1024.0);
		ASin1024LookupTable[i] = (int)((asinf(i / 1023.0) * 512.0) / RSDK_PI);
		ACos1024LookupTable[i] = (int)((acosf(i / 1023.0) * 512.0) / RSDK_PI);
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
		Sin512LookupTable[i] = (int)(sinf((i / 256.0) * RSDK_PI) * 512.0);
		Cos512LookupTable[i] = (int)(cosf((i / 256.0) * RSDK_PI) * 512.0);
		Tan512LookupTable[i] = (int)(tanf((i / 256.0) * RSDK_PI) * 512.0);
		ASin512LookupTable[i] = (int)((asinf(i / 511.0) * 256.0) / RSDK_PI);
		ACos512LookupTable[i] = (int)((acosf(i / 511.0) * 256.0) / RSDK_PI);
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
        ASin256LookupTable[i] = (int)((asinf(i / 255.0) * 128.0) / RSDK_PI);
        ACos256LookupTable[i] = (int)((acosf(i / 255.0) * 128.0) / RSDK_PI);
    }

    for (int y = 0; y < 0x100; y++) {
        unsigned int* arcTan = (unsigned int*)&ArcTan256LookupTable[y];

        for (int x = 0; x < 0x100; x++) {
            *arcTan = (int)(float)((float)atan2((float)y, x) * 40.743664f);
            arcTan += 0x100;
        }
    }
}
int Math::Sin1024(int angle) {
	return Sin1024LookupTable[angle & 0x3FF];
}
int Math::Cos1024(int angle) {
	return Cos1024LookupTable[angle & 0x3FF];
}
int Math::Tan1024(int angle) {
	return Tan1024LookupTable[angle & 0x3FF];
}
int Math::ASin1024(int angle) {
	if (angle > 0x3FF) {
		return 0;
	}
	if (angle < 0) {
		return -ASin1024LookupTable[-angle];
	}
	return ASin1024LookupTable[angle];
}
int Math::ACos1024(int angle) {
	if (angle > 0x3FF) {
		return 0;
	}
	if (angle < 0) {
		return -ACos1024LookupTable[-angle];
	}
	return ACos1024LookupTable[angle];
}
int Math::Sin512(int angle) {
	return Sin512LookupTable[angle & 0x1FF];
}
int Math::Cos512(int angle) {
	return Cos512LookupTable[angle & 0x1FF];
}
int Math::Tan512(int angle) {
	return Tan512LookupTable[angle & 0x1FF];
}
int Math::ASin512(int angle) {
	if (angle > 0x1FF) {
		return 0;
	}
	if (angle < 0) {
		return -ASin512LookupTable[-angle];
	}
	return ASin512LookupTable[angle];
}
int Math::ACos512(int angle) {
	if (angle > 0x1FF) {
		return 0;
	}
	if (angle < 0) {
		return -ACos512LookupTable[-angle];
	}
	return ACos512LookupTable[angle];
}
int Math::Sin256(int angle) {
	return Sin256LookupTable[angle & 0XFF];
}
int Math::Cos256(int angle) {
	return Cos256LookupTable[angle & 0XFF];
}
int Math::Tan256(int angle) {
	return Tan256LookupTable[angle & 0xFF];
}
int Math::ASin256(int angle) {
	if (angle > 0xFF) {
		return 0;
	}
	if (angle < 0) {
		return -ASin256LookupTable[-angle];
	}
	return ASin256LookupTable[angle];
}
int Math::ACos256(int angle) {
	if (angle > 0XFF) {
		return 0;
	}
	if (angle < 0) {
		return -ACos256LookupTable[-angle];
	}
	return ACos256LookupTable[angle];
}
unsigned int Math::ArcTanLookup(int X, int Y) {
    int x = abs(X);
    int y = abs(Y);

    if (x <= y) {
        while (y > 0xFF) {
            x >>= 4;
            y >>= 4;
        }
    }
    else {
        while (x > 0xFF) {
            x >>= 4;
            y >>= 4;
        }
    }
    if (X <= 0) {
        if (Y <= 0)
            return ArcTan256LookupTable[(x << 8) + y] + 0x80;
        else
            return 0x80 - ArcTan256LookupTable[(x << 8) + y];
    }
    else if (Y <= 0)
        return -ArcTan256LookupTable[(x << 8) + y];
    else
        return ArcTan256LookupTable[(x << 8) + y];
}

// help
int Math::CeilPOT(int n) {
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
float Math::Abs(float n) {
	return std::abs(n);
}
float Math::Max(float a, float b) {
	return a > b ? a : b;
}
float Math::Min(float a, float b) {
	return a < b ? a : b;
}
float Math::Clamp(float v, float a, float b) {
	return Math::Max(a, Math::Min(v, b));
}
float Math::Sign(float a) {
	return a < 0.0f ? -1.0f : a > 0.0f ? 1.0f : 0.0f;
}

// Random functions (non-inclusive)
float Math::Random() {
	return rand() / (float)RAND_MAX;
}
float Math::RandomMax(float max) {
	return Math::Random() * max;
}
float Math::RandomRange(float min, float max) {
	return (Math::Random() * (max - min)) + min;
}
int Math::RSDK_GetRandSeed() {
	return randSeed;
}
void Math::RSDK_SetRandSeed(int key) {
	randSeed = key;
}
int Math::RSDK_RandomInteger(int min, int max) {
	int seed1 = 1103515245 * randSeed + 12345;
	int seed2 = 1103515245 * seed1 + 12345;
	randSeed = 1103515245 * seed2 + 12345;

	int result = ((randSeed >> 16) & 0x7FF) ^
		((((seed1 >> 6) & 0x1FFC00) ^ ((seed2 >> 16) & 0x7FF)) << 10);
	int size = abs(max - min);

	if (min > max) {
		return (result - result / size * size + max);
	}
	else if (min < max) {
		return (result - result / size * size + min);
	}
	else {
		return max;
	}
}
int Math::RSDK_RandomIntegerSeeded(int min, int max, int seed) {
	if (!randSeed) {
		return 0;
	}

	int seed1 = 1103515245 * randSeed + 12345;
	int seed2 = 1103515245 * seed1 + 12345;
	randSeed = 1103515245 * seed2 + 12345;

	int result = ((randSeed >> 16) & 0x7FF) ^
		((((seed1 >> 6) & 0x1FFC00) ^ ((seed2 >> 16) & 0x7FF)) << 10);
	int size = abs(max - min);

	if (min > max) {
		return (result - result / size * size + max);
	}
	else if (min < max) {
		return (result - result / size * size + min);
	}
	else {
		return max;
	}
}

// Conversion functions
int Math::ToFixed(float val) {
    return (int)(val * 65536.0);
}
int Math::ToFixed(int val) {
    return val << 16;
}
float Math::FromFixed(int val) {
    return (float)(val / 65536.0);
}