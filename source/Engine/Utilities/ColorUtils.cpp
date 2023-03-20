#if INTERFACE
#include <Engine/Includes/Standard.h>

class ColorUtils {
public:
};
#endif

#include <Engine/Utilities/ColorUtils.h>

#define CLAMP_VAL(v, a, b) if (v < a) v = a; else if (v > b) v = b

PUBLIC STATIC Uint32 ColorUtils::ToRGB(int r, int g, int b) {
    CLAMP_VAL(r, 0x00, 0xFF);
    CLAMP_VAL(g, 0x00, 0xFF);
    CLAMP_VAL(b, 0x00, 0xFF);

    return 0xFF000000U | r << 16 | g << 8 | b;
}
PUBLIC STATIC Uint32 ColorUtils::ToRGB(float r, float g, float b) {
    int colR = (int)(r * 0xFF);
    int colG = (int)(g * 0xFF);
    int colB = (int)(b * 0xFF);

    return ColorUtils::ToRGB(colR, colG, colB);
}
PUBLIC STATIC Uint32 ColorUtils::ToRGB(float *r) {
    return ToRGB(r[0], r[1], r[2]);
}
PUBLIC STATIC Uint32 ColorUtils::Tint(Uint32 color, Uint32 colorMult) {
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
PUBLIC STATIC Uint32 ColorUtils::Tint(Uint32 color, Uint32 colorMult, Uint16 percentage) {
    return Blend(color, Tint(color, colorMult), percentage);
}
PUBLIC STATIC Uint32 ColorUtils::Multiply(Uint32 color, Uint32 colorMult) {
    Uint32 R = (((colorMult >> 16) & 0xFF) + 1) * (color & 0xFF0000);
    Uint32 G = (((colorMult >> 8) & 0xFF) + 1) * (color & 0x00FF00);
    Uint32 B = (((colorMult) & 0xFF) + 1) * (color & 0x0000FF);
    return (int)((R >> 8) | (G >> 8) | (B >> 8));
}
PUBLIC STATIC Uint32 ColorUtils::Blend(Uint32 color1, Uint32 color2, int percent) {
    Uint32 rb = color1 & 0xFF00FFU;
    Uint32 g  = color1 & 0x00FF00U;
    rb += (((color2 & 0xFF00FFU) - rb) * percent) >> 8;
    g  += (((color2 & 0x00FF00U) - g) * percent) >> 8;
    return (rb & 0xFF00FFU) | (g & 0x00FF00U);
}
PUBLIC STATIC void   ColorUtils::ConvertFromARGBtoABGR(Uint32* argb, int count) {
    Uint8* color = (Uint8*)argb;
    for (int p = 0; p < count; p++) {
        *argb = 0xFF000000U | color[0] << 16 | color[1] << 8 | color[2];
        color += 4;
        argb++;
    }
}
