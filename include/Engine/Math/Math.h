#ifndef ENGINE_MATH_MATH_H
#define ENGINE_MATH_MATH_H

#include <Engine/Includes/Standard.h>

namespace Math {
//public:
	void Init();
	float Cos(float n);
	float Sin(float n);
	float Tan(float n);
	float Asin(float x);
	float Acos(float x);
	float Atan(float x, float y);
	float Distance(float x1, float y1, float x2, float y2);
	float Hypot(float a, float b, float c);
	void ClearTrigLookupTables();
	void CalculateTrigAngles();
	int Sin1024(int angle);
	int Cos1024(int angle);
	int Tan1024(int angle);
	int ASin1024(int angle);
	int ACos1024(int angle);
	int Sin512(int angle);
	int Cos512(int angle);
	int Tan512(int angle);
	int ASin512(int angle);
	int ACos512(int angle);
	int Sin256(int angle);
	int Cos256(int angle);
	int Tan256(int angle);
	int ASin256(int angle);
	int ACos256(int angle);
	int CeilPOT(int n);
	float Abs(float n);
	float Max(float a, float b);
	float Min(float a, float b);
	float Clamp(float v, float a, float b);
	float Sign(float a);
	float Random();
	float RandomMax(float max);
	float RandomRange(float min, float max);
	int RSDK_GetRandSeed();
	void RSDK_SetRandSeed(int key);
	int RSDK_RandomInteger(int min, int max);
	int RSDK_RandomIntegerSeeded(int min, int max, int seed);
};

#endif /* ENGINE_MATH_MATH_H */
