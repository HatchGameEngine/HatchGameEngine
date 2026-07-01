#include <Engine/Math/Ease.h>

#include <Engine/Math/Math.h>

float Ease::InSine(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return 1.0 - cos(M_PI_HALF * t);
}
float Ease::OutSine(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return sin(M_PI_HALF * t);
}
float Ease::InOutSine(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return 0.5 * (1 + sin(M_PI * (t - 0.5)));
}
float Ease::InQuad(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return t * t;
}
float Ease::OutQuad(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return t * (2 - t);
}
float Ease::InOutQuad(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return t < 0.5 ? 2 * t * t : t * (4 - 2 * t) - 1;
}
float Ease::InCubic(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return t * t * t;
}
float Ease::OutCubic(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	t -= 1.0;
	return 1 + t * t * t;
}
float Ease::InOutCubic(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return t < 0.5 ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2;
}
float Ease::InQuart(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	t *= t;
	return t * t;
}
float Ease::OutQuart(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	t = (t - 1) * (t - 1);
	return 1 - t * t;
}
float Ease::InOutQuart(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	if (t < 0.5) {
		t *= t;
		return 8 * t * t;
	}
	else {
		t = (t - 1) * (t - 1);
		return 1 - 8 * t * t;
	}
}
float Ease::InQuint(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	float t2 = t * t;
	return t * t2 * t2;
}
float Ease::OutQuint(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	float t2 = (t - 1) * (t - 1);
	return 1 + t * t2 * t2;
}
float Ease::InOutQuint(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	float t2;
	if (t < 0.5) {
		t2 = t * t;
		return 16.0f * t * t2 * t2;
	}
	else {
		t = 1.0f - t;
		t2 = t * t;
		return 1.0f - 16.0f * t * t2 * t2;
	}
}
float Ease::InExpo(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return (pow(2, 8 * t) - 1) / 255;
}
float Ease::OutExpo(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return 1 - pow(2, -8 * t);
}
float Ease::InOutExpo(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	if (t < 0.5) {
		return (pow(2, 16 * t) - 1) / 510;
	}
	else {
		return 1 - 0.5 * pow(2, -16 * (t - 0.5));
	}
}
float Ease::InCirc(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return 1 - sqrt(1 - t);
}
float Ease::OutCirc(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return sqrt(t);
}
float Ease::InOutCirc(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	if (t < 0.5) {
		return (1 - sqrt(1 - 2 * t)) * 0.5;
	}
	else {
		return (1 + sqrt(2 * t - 1)) * 0.5;
	}
}
float Ease::InBack(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return t * t * (2.70158 * t - 1.70158);
}
float Ease::OutBack(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	t -= 1.0;
	return 1 + t * t * (2.70158 * t + 1.70158);
}
float Ease::InOutBack(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	if (t < 0.5) {
		return t * t * (7 * t - 2.5) * 2;
	}
	else {
		t -= 1.0;
		return 1 + t * t * 2 * (7 * t + 2.5);
	}
}
float Ease::InElastic(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	float t2 = t * t;
	return t2 * t2 * sin(t * M_PI * 4.5);
}
float Ease::OutElastic(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	float t2 = (t - 1) * (t - 1);
	return 1 - t2 * t2 * cos(t * M_PI * 4.5);
}
float Ease::InOutElastic(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	float t2;
	if (t < 0.45) {
		t2 = t * t;
		return 8 * t2 * t2 * sin(t * M_PI * 9);
	}
	else if (t < 0.55) {
		return 0.5 + 0.75 * sin(t * M_PI * 4);
	}
	else {
		t2 = (t - 1) * (t - 1);
		return 1 - 8 * t2 * t2 * sin(t * M_PI * 9);
	}
}
float Ease::InBounce(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return pow(2, 6 * (t - 1)) * abs(sin(t * M_PI * 3.5));
}
float Ease::OutBounce(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	return 1 - pow(2, -6 * t) * abs(cos(t * M_PI * 3.5));
}
float Ease::InOutBounce(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	if (t < 0.5) {
		return 8 * pow(2, 8 * (t - 1)) * abs(sin(t * M_PI * 7));
	}
	else {
		return 1 - 8 * pow(2, -8 * t) * abs(sin(t * M_PI * 7));
	}
}
float Ease::Triangle(float t) {
	t = Math::Clamp(t, 0.0f, 1.0f);
	if (t < 0.5) {
		return t * 2.0;
	}
	else {
		return 2.0 - t * 2.0;
	}
}

float Ease::Do(Ease::Mode easing, float t) {
	switch (easing) {
	case Ease::INSINE:
		return Ease::InSine(t);
	case Ease::OUTSINE:
		return Ease::OutSine(t);
	case Ease::INOUTSINE:
		return Ease::InOutSine(t);
	case Ease::INQUAD:
		return Ease::InQuad(t);
	case Ease::OUTQUAD:
		return Ease::OutQuad(t);
	case Ease::INOUTQUAD:
		return Ease::InOutQuad(t);
	case Ease::INCUBIC:
		return Ease::InCubic(t);
	case Ease::OUTCUBIC:
		return Ease::OutCubic(t);
	case Ease::INOUTCUBIC:
		return Ease::InOutCubic(t);
	case Ease::INQUART:
		return Ease::InQuart(t);
	case Ease::OUTQUART:
		return Ease::OutQuart(t);
	case Ease::INOUTQUART:
		return Ease::InOutQuart(t);
	case Ease::INQUINT:
		return Ease::InQuint(t);
	case Ease::OUTQUINT:
		return Ease::OutQuint(t);
	case Ease::INOUTQUINT:
		return Ease::InOutQuint(t);
	case Ease::INEXPO:
		return Ease::InExpo(t);
	case Ease::OUTEXPO:
		return Ease::OutExpo(t);
	case Ease::INOUTEXPO:
		return Ease::InOutExpo(t);
	case Ease::INCIRC:
		return Ease::InCirc(t);
	case Ease::OUTCIRC:
		return Ease::OutCirc(t);
	case Ease::INOUTCIRC:
		return Ease::InOutCirc(t);
	case Ease::INBACK:
		return Ease::InBack(t);
	case Ease::OUTBACK:
		return Ease::OutBack(t);
	case Ease::INOUTBACK:
		return Ease::InOutBack(t);
	case Ease::INELASTIC:
		return Ease::InElastic(t);
	case Ease::OUTELASTIC:
		return Ease::OutElastic(t);
	case Ease::INOUTELASTIC:
		return Ease::InOutElastic(t);
	case Ease::INBOUNCE:
		return Ease::InBounce(t);
	case Ease::OUTBOUNCE:
		return Ease::OutBounce(t);
	case Ease::INOUTBOUNCE:
		return Ease::InOutBounce(t);
	case Ease::TRIANGLE:
		return Ease::Triangle(t);
	default:
		return t;
	}
}
