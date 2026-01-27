#include <Engine/Rendering/Enums.h>
#include <Engine/Utilities/ColorUtils.h>

#define CLAMP_VAL(v, a, b) \
	if (v < a) \
		v = a; \
	else if (v > b) \
	v = b

Uint32 ColorUtils::ToRGB(int r, int g, int b) {
	return ToRGB(r, g, b, 0xFF);
}
Uint32 ColorUtils::ToRGB(int r, int g, int b, int a) {
	CLAMP_VAL(r, 0x00, 0xFF);
	CLAMP_VAL(g, 0x00, 0xFF);
	CLAMP_VAL(b, 0x00, 0xFF);
	CLAMP_VAL(a, 0x00, 0xFF);

#if HATCH_BIG_ENDIAN
	Uint32 color = r << 24 | g << 16 | b << 8 | a;
#else
	Uint32 color = a << 24 | b << 16 | g << 8 | r;
#endif

	return color;
}
Uint32 ColorUtils::ToRGB(float r, float g, float b) {
	int colR = (int)(r * 0xFF);
	int colG = (int)(g * 0xFF);
	int colB = (int)(b * 0xFF);

	return ColorUtils::ToRGB(colR, colG, colB);
}
Uint32 ColorUtils::ToRGB(float r, float g, float b, float a) {
	int colR = (int)(r * 0xFF);
	int colG = (int)(g * 0xFF);
	int colB = (int)(b * 0xFF);
	int colA = (int)(a * 0xFF);

	return ColorUtils::ToRGB(colR, colG, colB, colA);
}
Uint32 ColorUtils::ToRGB(float* r) {
	return ToRGB(r[0], r[1], r[2]);
}
Uint32 ColorUtils::ToRGBA(float* r) {
	return ToRGB(r[0], r[1], r[2], r[3]);
}
void ColorUtils::SeparateRGB(Uint32 color, float* dest) {
#if HATCH_BIG_ENDIAN
	Uint8 red = (color >> 24) & 0xFF;
	Uint8 green = (color >> 16) & 0xFF;
	Uint8 blue = (color >> 8) & 0xFF;
#else
	Uint8 red = color & 0xFF;
	Uint8 green = (color >> 8) & 0xFF;
	Uint8 blue = (color >> 16) & 0xFF;
#endif

	dest[0] = red / 255.f;
	dest[1] = green / 255.f;
	dest[2] = blue / 255.f;
}
void ColorUtils::Separate(Uint32 color, float* dest) {
#if HATCH_BIG_ENDIAN
	Uint8 red = (color >> 24) & 0xFF;
	Uint8 green = (color >> 16) & 0xFF;
	Uint8 blue = (color >> 8) & 0xFF;
	Uint8 alpha = color & 0xFF;
#else
	Uint8 red = color & 0xFF;
	Uint8 green = (color >> 8) & 0xFF;
	Uint8 blue = (color >> 16) & 0xFF;
	Uint8 alpha = (color >> 24) & 0xFF;
#endif

	dest[0] = red / 255.f;
	dest[1] = green / 255.f;
	dest[2] = blue / 255.f;
	dest[3] = alpha / 255.f;
}
void ColorUtils::SeparateRGB(Uint32 color, Uint8* dest) {
#if HATCH_BIG_ENDIAN
	Uint8 red = (color >> 24) & 0xFF;
	Uint8 green = (color >> 16) & 0xFF;
	Uint8 blue = (color >> 8) & 0xFF;
#else
	Uint8 red = color & 0xFF;
	Uint8 green = (color >> 8) & 0xFF;
	Uint8 blue = (color >> 16) & 0xFF;
#endif

	dest[0] = red;
	dest[1] = green;
	dest[2] = blue;
}
void ColorUtils::Separate(Uint32 color, Uint8* dest) {
#if HATCH_BIG_ENDIAN
	Uint8 red = (color >> 24) & 0xFF;
	Uint8 green = (color >> 16) & 0xFF;
	Uint8 blue = (color >> 8) & 0xFF;
	Uint8 alpha = color & 0xFF;
#else
	Uint8 red = color & 0xFF;
	Uint8 green = (color >> 8) & 0xFF;
	Uint8 blue = (color >> 16) & 0xFF;
	Uint8 alpha = (color >> 24) & 0xFF;
#endif

	dest[0] = red;
	dest[1] = green;
	dest[2] = blue;
	dest[3] = alpha;
}
void ColorUtils::SeparateRGB(float* color, Uint8* dest) {
	dest[0] = color[0] * 0xFF;
	dest[1] = color[1] * 0xFF;
	dest[2] = color[2] * 0xFF;
}
void ColorUtils::Separate(float* color, Uint8* dest) {
	dest[0] = color[0] * 0xFF;
	dest[1] = color[1] * 0xFF;
	dest[2] = color[2] * 0xFF;
	dest[2] = color[3] * 0xFF;
}
Uint32 ColorUtils::Tint(Uint32 color, Uint32 colorMult) {
	Uint32 dR = colorMult & 0xFF;
	Uint32 dG = (colorMult >> 8) & 0xFF;
	Uint32 dB = (colorMult >> 16) & 0xFF;
	Uint32 sR = color & 0xFF;
	Uint32 sG = (color >> 8) & 0xFF;
	Uint32 sB = (color >> 16) & 0xFF;
	dR = (Uint8)((dR * sR + 0xFF) >> 8);
	dG = (Uint8)((dG * sG + 0xFF) >> 8);
	dB = (Uint8)((dB * sB + 0xFF) >> 8);
	return dB | (dG << 8) | (dR << 16);
}
Uint32 ColorUtils::Tint(Uint32 color, Uint32 colorMult, Uint16 percentage) {
	return Blend(color, Tint(color, colorMult), percentage);
}
Uint32 ColorUtils::Multiply(Uint32 color, Uint32 colorMult) {
	Uint32 R = ((colorMult & 0xFF) + 1) * (color & 0x0000FF);
	Uint32 G = (((colorMult >> 8) & 0xFF) + 1) * (color & 0x00FF00);
	Uint32 B = (((colorMult >> 16) & 0xFF) + 1) * (color & 0xFF0000);
	return (int)((R >> 8) | (G >> 8) | (B >> 8));
}
Uint32 ColorUtils::Blend(Uint32 color1, Uint32 color2, int percent) {
	Uint32 br = color1 & 0xFF00FFU;
	Uint32 g = color1 & 0x00FF00U;
	br += (((color2 & 0xFF00FFU) - br) * percent) >> 8;
	g += (((color2 & 0x00FF00U) - g) * percent) >> 8;
	return (br & 0xFF00FFU) | (g & 0x00FF00U);
}
Uint32 ColorUtils::Make(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha, int pixelFormat) {
	Uint32 color = 0;

	switch (pixelFormat) {
	case PixelFormat_RGBA8888:
#if HATCH_BIG_ENDIAN
		color = (red << 24) | (green << 16) | (blue << 8) | alpha;
#else
		color = (alpha << 24) | (blue << 16) | (green << 8) | red;
#endif
		break;
	case PixelFormat_ABGR8888:
#if HATCH_BIG_ENDIAN
		color = (alpha << 24) | (blue << 16) | (green << 8) | red;
#else
		color = (red << 24) | (green << 16) | (blue << 8) | alpha;
#endif
		break;
	case PixelFormat_ARGB8888:
#if HATCH_BIG_ENDIAN
		color = (alpha << 24) | (red << 16) | (green << 8) | blue;
#else
		color = (blue << 24) | (green << 16) | (red << 8) | alpha;
#endif
		break;
	case PixelFormat_RGB888:
#if HATCH_BIG_ENDIAN
		color = (red << 16) | (green << 8) | blue;
#else
		color = (blue << 16) | (green << 8) | red;
#endif
		break;
	case PixelFormat_BGR888:
#if HATCH_BIG_ENDIAN
		color = (blue << 16) | (green << 8) | red;
#else
		color = (red << 16) | (green << 8) | blue;
#endif
		break;
	}

	return color;
}
Uint32 ColorUtils::Convert(Uint32 color, int srcPixelFormat, int destPixelFormat) {
	if (srcPixelFormat == destPixelFormat) {
		return color;
	}

	Uint8 red = 0;
	Uint8 green = 0;
	Uint8 blue = 0;
	Uint8 alpha = 0;

	switch (srcPixelFormat) {
	case PixelFormat_RGBA8888:
#if HATCH_BIG_ENDIAN
		red = (color >> 24) & 0xFF;
		green = (color >> 16) & 0xFF;
		blue = (color >> 8) & 0xFF;
		alpha = color & 0xFF;
#else
		red = color & 0xFF;
		green = (color >> 8) & 0xFF;
		blue = (color >> 16) & 0xFF;
		alpha = (color >> 24) & 0xFF;
#endif
		break;
	case PixelFormat_ARGB8888:
#if HATCH_BIG_ENDIAN
		alpha = (color >> 24) & 0xFF;
		red = (color >> 16) & 0xFF;
		green = (color >> 8) & 0xFF;
		blue = color & 0xFF;
#else
		red = color & 0xFF;
		green = (color >> 8) & 0xFF;
		blue = (color >> 16) & 0xFF;
		alpha = (color >> 24) & 0xFF;
#endif
		break;
	case PixelFormat_ABGR8888:
#if HATCH_BIG_ENDIAN
		alpha = (color >> 24) & 0xFF;
		blue = (color >> 16) & 0xFF;
		green = (color >> 8) & 0xFF;
		red = color & 0xFF;
#else
		alpha = color & 0xFF;
		blue = (color >> 8) & 0xFF;
		green = (color >> 16) & 0xFF;
		red = (color >> 24) & 0xFF;
#endif
		break;
	case PixelFormat_RGB888:
#if HATCH_BIG_ENDIAN
		red = (color >> 24) & 0xFF;
		green = (color >> 16) & 0xFF;
		blue = (color >> 8) & 0xFF;
#else
		red = color & 0xFF;
		green = (color >> 8) & 0xFF;
		blue = (color >> 16) & 0xFF;
#endif
		alpha = 0xFF;
		break;
	case PixelFormat_BGR888:
#if HATCH_BIG_ENDIAN
		blue = (color >> 24) & 0xFF;
		green = (color >> 16) & 0xFF;
		red = (color >> 8) & 0xFF;
#else
		blue = color & 0xFF;
		green = (color >> 8) & 0xFF;
		red = (color >> 16) & 0xFF;
#endif
		alpha = 0xFF;
		break;
	}

	return ColorUtils::Make(red, green, blue, alpha, destPixelFormat);
}
void ColorUtils::Convert(Uint32* colors, int count, int srcPixelFormat, int destPixelFormat) {
	if (srcPixelFormat == destPixelFormat) {
		return;
	}

	for (int p = 0; p < count; p++) {
		*colors = ColorUtils::Convert(*colors, srcPixelFormat, destPixelFormat);
		colors++;
	}
}
int ColorUtils::NearestColor(Uint8 r, Uint8 g, Uint8 b, Uint32* palette, unsigned numColors) {
	Sint64 minDist = 255 * 255 * 3;

	int bestColor = 0;

	for (unsigned i = 0; i < numColors; i++) {
		Uint32 color = palette[i];

		Sint64 diffR = ((Sint64)r) - (color & 0xFF);
		Sint64 diffG = ((Sint64)g) - ((color >> 8) & 0xFF);
		Sint64 diffB = ((Sint64)b) - ((color >> 16) & 0xFF);

		Sint64 dist = diffR * diffR + diffG * diffG + diffB * diffB;
		if (dist < minDist) {
			bestColor = (int)i;
			if (!dist) {
				return bestColor;
			}

			minDist = dist;
		}
	}

	return bestColor;
}
