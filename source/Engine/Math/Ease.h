#ifndef ENGINE_MATH_EASE_H
#define ENGINE_MATH_EASE_H

#include <Engine/Includes/Standard.h>

class Ease {
public:
	static float InSine(float t);
	static float OutSine(float t);
	static float InOutSine(float t);
	static float InQuad(float t);
	static float OutQuad(float t);
	static float InOutQuad(float t);
	static float InCubic(float t);
	static float OutCubic(float t);
	static float InOutCubic(float t);
	static float InQuart(float t);
	static float OutQuart(float t);
	static float InOutQuart(float t);
	static float InQuint(float t);
	static float OutQuint(float t);
	static float InOutQuint(float t);
	static float InExpo(float t);
	static float OutExpo(float t);
	static float InOutExpo(float t);
	static float InCirc(float t);
	static float OutCirc(float t);
	static float InOutCirc(float t);
	static float InBack(float t);
	static float OutBack(float t);
	static float InOutBack(float t);
	static float InElastic(float t);
	static float OutElastic(float t);
	static float InOutElastic(float t);
	static float InBounce(float t);
	static float OutBounce(float t);
	static float InOutBounce(float t);
	static float Triangle(float t);
};

#endif /* ENGINE_MATH_EASE_H */
