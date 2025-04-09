#ifndef ENGINE_RENDERING_SOFTWARE_SCANLINE_H
#define ENGINE_RENDERING_SOFTWARE_SCANLINE_H

#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Rendering/Texture.h>

namespace Scanline {
//public:
	void Prepare(int y1, int y2);
	void Process(int x1, int y1, int x2, int y2);
	void Process(int color1, int color2, int x1, int y1, int x2, int y2);
	void ProcessDepth(int x1, int y1, int z1, int x2, int y2, int z2);
	void
	ProcessDepth(int color1, int color2, int x1, int y1, int z1, int x2, int y2, int z2);
	void
	ProcessUVAffine(Vector2 uv1, Vector2 uv2, int x1, int y1, int z1, int x2, int y2, int z2);
	void ProcessUVAffine(int color1,
		int color2,
		Vector2 uv1,
		Vector2 uv2,
		int x1,
		int y1,
		int z1,
		int x2,
		int y2,
		int z2);
	void
	ProcessUV(Vector2 uv1, Vector2 uv2, int x1, int y1, int z1, int x2, int y2, int z2);
	void ProcessUV(int color1,
		int color2,
		Vector2 uv1,
		Vector2 uv2,
		int x1,
		int y1,
		int z1,
		int x2,
		int y2,
		int z2);
};

#endif /* ENGINE_RENDERING_SOFTWARE_SCANLINE_H */
