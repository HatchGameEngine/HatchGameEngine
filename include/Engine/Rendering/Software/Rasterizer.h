#ifndef ENGINE_RENDERING_SOFTWARE_RASTERIZER_H
#define ENGINE_RENDERING_SOFTWARE_RASTERIZER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Rendering/Material.h>

class Rasterizer {
public:
    static void Prepare(int y1, int y2);
    static void Scanline(int x1, int y1, int x2, int y2);
    static void Scanline(int color1, int color2, int x1, int y1, int x2, int y2);
    static void ScanlineDepth(int x1, int y1, int z1, int x2, int y2, int z2);
    static void ScanlineDepth(int color1, int color2, int x1, int y1, int z1, int x2, int y2, int z2);
    static void ScanlineUVAffine(Vector2 uv1, Vector2 uv2, int x1, int y1, int z1, int x2, int y2, int z2);
    static void ScanlineUVAffine(int color1, int color2, Vector2 uv1, Vector2 uv2, int x1, int y1, int z1, int x2, int y2, int z2);
    static void ScanlineUV(Vector2 uv1, Vector2 uv2, int x1, int y1, int z1, int x2, int y2, int z2);
    static void ScanlineUV(int color1, int color2, Vector2 uv1, Vector2 uv2, int x1, int y1, int z1, int x2, int y2, int z2);
};

#endif /* ENGINE_RENDERING_SOFTWARE_RASTERIZER_H */
