#ifndef ENGINE_MATH_MATH_H
#define ENGINE_MATH_MATH_H

#include <Engine/Includes/Standard.h>

class Math {
public:
	static void Init();
	constexpr static float Cos(float n){
		return std::cos(n);
	}
	constexpr static float Sin(float n){
		return std::sin(n);
	}
	static float Tan(float n);
	static float Asin(float x);
	static float Acos(float x);
	static float Atan(float x, float y);
	static float Distance(float x1, float y1, float x2, float y2);
	static float Hypot(float a, float b, float c);
	static void ClearTrigLookupTables();
	static void CalculateTrigAngles();
	static int Sin1024(int angle);
	static int Cos1024(int angle);
	static int Tan1024(int angle);
	static int ASin1024(int angle);
	static int ACos1024(int angle);
	static int Sin512(int angle);
	static int Cos512(int angle);
	static int Tan512(int angle);
	static int ASin512(int angle);
	static int ACos512(int angle);
	static int Sin256(int angle);
	static int Cos256(int angle);
	static int Tan256(int angle);
	static int ASin256(int angle);
	static int ACos256(int angle);
	static int CeilPOT(int n);
	static float Abs(float n);
	static float Max(float a, float b);
	static float Min(float a, float b);
	static float Clamp(float v, float a, float b);
	static float Sign(float a);
	static float Random();
	static float RandomMax(float max);
	static float RandomRange(float min, float max);
	static int RSDK_GetRandSeed();
	static void RSDK_SetRandSeed(int key);
	static int RSDK_RandomInteger(int min, int max);
	static int RSDK_RandomIntegerSeeded(int min, int max, int seed);
};

#endif /* ENGINE_MATH_MATH_H */
