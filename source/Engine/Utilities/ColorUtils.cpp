#include <Engine/Utilities/ColorUtils.h>

#define CLAMP_VAL(v, a, b) \
	if (v < a) \
		v = a; \
	else if (v > b) \
	v = b

Uint32 ColorUtils::ToRGB(int r, int g, int b) {
	CLAMP_VAL(r, 0x00, 0xFF);
	CLAMP_VAL(g, 0x00, 0xFF);
	CLAMP_VAL(b, 0x00, 0xFF);

	return 0xFF000000U | r << 16 | g << 8 | b;
}
Uint32 ColorUtils::ToRGB(int r, int g, int b, int a) {
	CLAMP_VAL(r, 0x00, 0xFF);
	CLAMP_VAL(g, 0x00, 0xFF);
	CLAMP_VAL(b, 0x00, 0xFF);
	CLAMP_VAL(a, 0x00, 0xFF);

	return a << 24 | r << 16 | g << 8 | b;
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
	dest[0] = (color >> 16 & 0xFF) / 255.f;
	dest[1] = (color >> 8 & 0xFF) / 255.f;
	dest[2] = (color & 0xFF) / 255.f;
}
void ColorUtils::Separate(Uint32 color, float* dest) {
	dest[3] = (color >> 24 & 0xFF) / 255.f;
	SeparateRGB(color, dest);
}
void ColorUtils::SeparateRGB(Uint32 color, Uint8* dest) {
	dest[0] = (color >> 16) & 0xFF;
	dest[1] = (color >> 8) & 0xFF;
	dest[2] = color & 0xFF;
}
void ColorUtils::Separate(Uint32 color, Uint8* dest) {
	dest[3] = (color >> 24) & 0xFF;
	SeparateRGB(color, dest);
}
void ColorUtils::SeparateRGB(float* color, Uint8* dest) {
	dest[0] = color[0] * 0xFF;
	dest[1] = color[1] * 0xFF;
	dest[2] = color[2] * 0xFF;
}
void ColorUtils::Separate(float* color, Uint8* dest) {
	dest[2] = color[3] * 0xFF;
	SeparateRGB(color, dest);
}
Uint32 ColorUtils::Tint(Uint32 color, Uint32 colorMult) {
	Uint32 dR = (colorMult >> 16) & 0xFF;
	Uint32 dG = (colorMult >> 8) & 0xFF;
	Uint32 dB = colorMult & 0xFF;
	Uint32 sR = (color >> 16) & 0xFF;
	Uint32 sG = (color >> 8) & 0xFF;
	Uint32 sB = color & 0xFF;
	dR = (Uint8)((dR * sR + 0xFF) >> 8);
	dG = (Uint8)((dG * sG + 0xFF) >> 8);
	dB = (Uint8)((dB * sB + 0xFF) >> 8);
	return dB | (dG << 8) | (dR << 16);
}
Uint32 ColorUtils::Tint(Uint32 color, Uint32 colorMult, Uint16 percentage) {
	return Blend(color, Tint(color, colorMult), percentage);
}
Uint32 ColorUtils::Multiply(Uint32 color, Uint32 colorMult) {
	Uint32 R = (((colorMult >> 16) & 0xFF) + 1) * (color & 0xFF0000);
	Uint32 G = (((colorMult >> 8) & 0xFF) + 1) * (color & 0x00FF00);
	Uint32 B = (((colorMult) & 0xFF) + 1) * (color & 0x0000FF);
	return (int)((R >> 8) | (G >> 8) | (B >> 8));
}
Uint32 ColorUtils::Blend(Uint32 color1, Uint32 color2, int percent) {
	Uint32 rb = color1 & 0xFF00FFU;
	Uint32 g = color1 & 0x00FF00U;
	rb += (((color2 & 0xFF00FFU) - rb) * percent) >> 8;
	g += (((color2 & 0x00FF00U) - g) * percent) >> 8;
	return (rb & 0xFF00FFU) | (g & 0x00FF00U);
}
void ColorUtils::ConvertFromARGBtoABGR(Uint32* argb, int count) {
	for (int p = 0; p < count; p++) {
		Uint8 red = (*argb >> 16) & 0xFF;
		Uint8 green = (*argb >> 8) & 0xFF;
		Uint8 blue = *argb & 0xFF;
		*argb = 0xFF000000U | blue << 16 | green << 8 | red;
		argb++;
	}
}
void ColorUtils::ConvertFromABGRtoARGB(Uint32* argb, int count) {
	for (int p = 0; p < count; p++) {
		Uint8 blue = (*argb >> 16) & 0xFF;
		Uint8 green = (*argb >> 8) & 0xFF;
		Uint8 red = *argb & 0xFF;
		*argb = 0xFF000000U | red << 16 | green << 8 | blue;
		argb++;
	}
}
int ColorUtils::NearestColor(Uint8 r, Uint8 g, Uint8 b, Uint32* palette, unsigned numColors) {
	Sint64 minDist = 255 * 255 * 3;

	int bestColor = 0;

	for (unsigned i = 0; i < numColors; i++) {
		Uint32 color = palette[i];

		Sint64 diffR = ((Sint64)r) - ((color >> 16) & 0xFF);
		Sint64 diffG = ((Sint64)g) - ((color >> 8) & 0xFF);
		Sint64 diffB = ((Sint64)b) - (color & 0xFF);

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
