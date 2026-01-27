#ifndef ENGINE_UTILITIES_COLORUTILS_H
#define ENGINE_UTILITIES_COLORUTILS_H

#include <Engine/Includes/Standard.h>

class ColorUtils {
public:
	static Uint32 ToRGB(int r, int g, int b);
	static Uint32 ToRGB(int r, int g, int b, int a);
	static Uint32 ToRGB(float r, float g, float b);
	static Uint32 ToRGB(float r, float g, float b, float a);
	static Uint32 ToRGB(float* r);
	static Uint32 ToRGBA(float* r);
	static void SeparateRGB(Uint32 color, float* dest);
	static void Separate(Uint32 color, float* dest);
	static void SeparateRGB(Uint32 color, Uint8* dest);
	static void Separate(Uint32 color, Uint8* dest);
	static void SeparateRGB(float* color, Uint8* dest);
	static void Separate(float* color, Uint8* dest);
	static Uint32 Tint(Uint32 color, Uint32 colorMult);
	static Uint32 Tint(Uint32 color, Uint32 colorMult, Uint16 percentage);
	static Uint32 Multiply(Uint32 color, Uint32 colorMult);
	static Uint32 Blend(Uint32 color1, Uint32 color2, int percent);
	static Uint32 Make(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha, int pixelFormat);
	static Uint32 Convert(Uint32 color, int srcPixelFormat, int destPixelFormat);
	static void Convert(Uint32* colors, int count, int srcPixelFormat, int destPixelFormat);
	static int NearestColor(Uint8 r, Uint8 g, Uint8 b, Uint32* palette, unsigned numColors);
};

#endif /* ENGINE_UTILITIES_COLORUTILS_H */
