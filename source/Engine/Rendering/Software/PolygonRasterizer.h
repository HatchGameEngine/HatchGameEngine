#ifndef ENGINE_RENDERING_SOFTWARE_POLYGONRASTERIZER_H
#define ENGINE_RENDERING_SOFTWARE_POLYGONRASTERIZER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Rendering/Texture.h>

class PolygonRasterizer {
public:
	static bool DepthTest;
	static size_t DepthBufferSize;
	static Uint32* DepthBuffer;
	static bool UseDepthBuffer;
	static bool UseFog;
	static float FogStart;
	static float FogEnd;
	static float FogDensity;
	static int FogColor;
	static Uint16 FogTable[0x100 + 1];

	static void DrawBasic(Vector2* positions, Uint32 color, int count, BlendState blendState);
	static void
	DrawBasicBlend(Vector2* positions, int* colors, int count, BlendState blendState);
	static void DrawShaded(Vector3* positions, Uint32 color, int count, BlendState blendState);
	static void
	DrawBlendShaded(Vector3* positions, int* colors, int count, BlendState blendState);
	static void DrawAffine(Texture* texture,
		Vector3* positions,
		Vector2* uvs,
		Uint32 color,
		int count,
		BlendState blendState);
	static void DrawBlendAffine(Texture* texture,
		Vector3* positions,
		Vector2* uvs,
		int* colors,
		int count,
		BlendState blendState);
	static void DrawPerspective(Texture* texture,
		Vector3* positions,
		Vector2* uvs,
		Uint32 color,
		int count,
		BlendState blendState);
	static void DrawBlendPerspective(Texture* texture,
		Vector3* positions,
		Vector2* uvs,
		int* colors,
		int count,
		BlendState blendState);
	static void DrawDepth(Vector3* positions, Uint32 color, int count, BlendState blendState);
	static void
	DrawBlendDepth(Vector3* positions, int* colors, int count, BlendState blendState);
	static void SetDepthTest(bool enabled);
	static void FreeDepthBuffer(void);
	static void SetUseDepthBuffer(bool enabled);
	static void SetUseFog(bool enabled);
	static void SetFogEquation(FogEquation equation);
	static void SetFogStart(float start);
	static void SetFogEnd(float end);
	static void SetFogDensity(float density);
	static void SetFogColor(float r, float g, float b);
	static void SetFogSmoothness(float smoothness);
};

#endif /* ENGINE_RENDERING_SOFTWARE_POLYGONRASTERIZER_H */
