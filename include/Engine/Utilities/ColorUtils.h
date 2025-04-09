#ifndef ENGINE_UTILITIES_COLORUTILS_H
#define ENGINE_UTILITIES_COLORUTILS_H

#include <Engine/Includes/Standard.h>

namespace ColorUtils {
//public:
	Uint32 ToRGB(int r, int g, int b);
	Uint32 ToRGB(int r, int g, int b, int a);
	Uint32 ToRGB(float r, float g, float b);
	Uint32 ToRGB(float r, float g, float b, float a);
	Uint32 ToRGB(float* r);
	Uint32 ToRGBA(float* r);
	void SeparateRGB(Uint32 color, float* dest);
	void Separate(Uint32 color, float* dest);
	void SeparateRGB(Uint32 color, Uint8* dest);
	void Separate(Uint32 color, Uint8* dest);
	void SeparateRGB(float* color, Uint8* dest);
	void Separate(float* color, Uint8* dest);
	Uint32 Tint(Uint32 color, Uint32 colorMult);
	Uint32 Tint(Uint32 color, Uint32 colorMult, Uint16 percentage);
	Uint32 Multiply(Uint32 color, Uint32 colorMult);
	Uint32 Blend(Uint32 color1, Uint32 color2, int percent);
	void ConvertFromARGBtoABGR(Uint32* argb, int count);
	void ConvertFromABGRtoARGB(Uint32* argb, int count);
	int NearestColor(Uint8 r, Uint8 g, Uint8 b, Uint32* palette, unsigned numColors);
};

#endif /* ENGINE_UTILITIES_COLORUTILS_H */
