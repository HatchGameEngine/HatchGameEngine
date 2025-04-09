#ifndef ENGINE_RENDERING_SOFTWARE_POLYGONRASTERIZER_H
#define ENGINE_RENDERING_SOFTWARE_POLYGONRASTERIZER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Rendering/Texture.h>

namespace PolygonRasterizer {
//public:
	extern bool DepthTest;
	extern size_t DepthBufferSize;
	extern Uint32* DepthBuffer;
	extern bool UseDepthBuffer;
	extern bool UseFog;
	extern float FogStart;
	extern float FogEnd;
	extern float FogDensity;
	extern int FogColor;
	extern Uint16 FogTable[0x100 + 1];

	void DrawBasic(Vector2* positions, Uint32 color, int count, BlendState blendState);
	void
	DrawBasicBlend(Vector2* positions, int* colors, int count, BlendState blendState);
	void DrawShaded(Vector3* positions, Uint32 color, int count, BlendState blendState);
	void
	DrawBlendShaded(Vector3* positions, int* colors, int count, BlendState blendState);
	void DrawAffine(Texture* texture,
		Vector3* positions,
		Vector2* uvs,
		Uint32 color,
		int count,
		BlendState blendState);
	void DrawBlendAffine(Texture* texture,
		Vector3* positions,
		Vector2* uvs,
		int* colors,
		int count,
		BlendState blendState);
	void DrawPerspective(Texture* texture,
		Vector3* positions,
		Vector2* uvs,
		Uint32 color,
		int count,
		BlendState blendState);
	void DrawBlendPerspective(Texture* texture,
		Vector3* positions,
		Vector2* uvs,
		int* colors,
		int count,
		BlendState blendState);
	void DrawDepth(Vector3* positions, Uint32 color, int count, BlendState blendState);
	void
	DrawBlendDepth(Vector3* positions, int* colors, int count, BlendState blendState);
	void SetDepthTest(bool enabled);
	void FreeDepthBuffer(void);
	void SetUseDepthBuffer(bool enabled);
	void SetUseFog(bool enabled);
	void SetFogEquation(FogEquation equation);
	void SetFogStart(float start);
	void SetFogEnd(float end);
	void SetFogDensity(float density);
	void SetFogColor(float r, float g, float b);
	void SetFogSmoothness(float smoothness);
};

#endif /* ENGINE_RENDERING_SOFTWARE_POLYGONRASTERIZER_H */
