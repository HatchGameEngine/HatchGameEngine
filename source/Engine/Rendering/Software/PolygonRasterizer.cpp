#include <Engine/Rendering/Software/Contour.h>
#include <Engine/Rendering/Software/PolygonRasterizer.h>
#include <Engine/Rendering/Software/Scanline.h>
#include <Engine/Rendering/Software/SoftwareEnums.h>
#include <Engine/Rendering/Software/SoftwareRenderer.h>
#include <Engine/Utilities/ColorUtils.h>

bool PolygonRasterizer::DepthTest = false;
size_t PolygonRasterizer::DepthBufferSize = 0;
Uint32* PolygonRasterizer::DepthBuffer = NULL;

bool PolygonRasterizer::UseDepthBuffer = false;

bool PolygonRasterizer::UseFog = false;
float PolygonRasterizer::FogStart = 0.0f;
float PolygonRasterizer::FogEnd = 1.0f;
float PolygonRasterizer::FogDensity = 1.0f;
int PolygonRasterizer::FogColor = 0x000000;
Uint16 PolygonRasterizer::FogTable[0x100 + 1];

#define DEPTH_READ_DUMMY(depth) true
#define DEPTH_WRITE_DUMMY(depth)

#define DEPTH_READ_U32(depth) (depth < PolygonRasterizer::DepthBuffer[dst_x + dst_strideY])
#define DEPTH_WRITE_U32(depth) PolygonRasterizer::DepthBuffer[dst_x + dst_strideY] = depth

#define CLAMP_VAL(v, a, b) \
	if (v < a) \
		v = a; \
	else if (v > b) \
	v = b

int FogEquationFunc_Linear(float coord) {
	float start = PolygonRasterizer::FogStart;
	float end = PolygonRasterizer::FogEnd;
	float fog;

	coord = 1.0f - (1.0f / coord);

	if (coord < 0.0f) {
		coord = 0.0f;
	}
	else if (coord > 1.0f) {
		coord = 1.0f;
	}

	fog = (end - coord) / (end - start);

	int result = fog * 0x100;
	CLAMP_VAL(result, 0, 0x100);
	return 0x100 - result;
}
int FogEquationFunc_Exp(float coord) {
	int result = expf(-PolygonRasterizer::FogDensity * coord) * 0x100;
	CLAMP_VAL(result, 0, 0x100);
	return 0x100 - result;
}

int (*FogEquationFunc)(float) = FogEquationFunc_Linear;

Uint32 DoFogLighting(int color, float fogCoord) {
	int fogValue = FogEquationFunc(fogCoord / 192.0f);
	if (fogValue != 0) {
		fogValue = PolygonRasterizer::FogTable[fogValue];
		color = ColorUtils::Blend(color, PolygonRasterizer::FogColor, fogValue);
	}
	return color | 0xFF000000U;
}
Uint32 DoColorTint(Uint32 color, Uint32 colorMult) {
	return ColorUtils::Tint(color, colorMult) | 0xFF000000U;
}

#define POLYGON_BLENDFLAGS_SOLID(drawPolygonMacro, pixelMacro) drawPolygonMacro(pixelMacro)

#define POLYGON_BLENDFLAGS_SOLID_DEPTH(drawPolygonMacro, dpR, dpW) drawPolygonMacro(dpR, dpW)

#define POLYGON_BLENDFLAGS_DEPTH(drawPolygonMacro, placePixelMacro, dpR, dpW) \
	drawPolygonMacro(placePixelMacro, dpR, dpW)

#define DRAW_POLYGON_SCANLINE_DEPTH(drawPolygonMacro, placePixel, placePixelPaletted) \
	if (Graphics::UsePalettes && texture->Paletted) { \
		if (UseDepthBuffer) { \
			POLYGON_BLENDFLAGS_DEPTH(drawPolygonMacro, \
				placePixelPaletted, \
				DEPTH_READ_U32, \
				DEPTH_WRITE_U32); \
		} \
		else { \
			POLYGON_BLENDFLAGS_DEPTH(drawPolygonMacro, \
				placePixelPaletted, \
				DEPTH_READ_DUMMY, \
				DEPTH_WRITE_DUMMY); \
		} \
	} \
	else { \
		if (UseDepthBuffer) { \
			POLYGON_BLENDFLAGS_DEPTH( \
				drawPolygonMacro, placePixel, DEPTH_READ_U32, DEPTH_WRITE_U32); \
		} \
		else { \
			POLYGON_BLENDFLAGS_DEPTH(drawPolygonMacro, \
				placePixel, \
				DEPTH_READ_DUMMY, \
				DEPTH_WRITE_DUMMY); \
		} \
	}

#define POLYGON_SCANLINE_DEPTH(drawPolygonMacro) \
	if (UseFog) { \
		DRAW_POLYGON_SCANLINE_DEPTH( \
			drawPolygonMacro, DRAW_PLACEPIXEL_FOG, DRAW_PLACEPIXEL_PAL_FOG) \
	} \
	else { \
		DRAW_POLYGON_SCANLINE_DEPTH( \
			drawPolygonMacro, DRAW_PLACEPIXEL, DRAW_PLACEPIXEL_PAL) \
	}

#define SCANLINE_INIT_Z() \
	contZ = contour.MinZ; \
	dxZ = (contour.MaxZ - contZ) / contLen
#define SCANLINE_INIT_RGB() \
	contR = contour.MinR; \
	contG = contour.MinG; \
	contB = contour.MinB; \
	dxR = (contour.MaxR - contR) / contLen; \
	dxG = (contour.MaxG - contG) / contLen; \
	dxB = (contour.MaxB - contB) / contLen
#define SCANLINE_INIT_UV() \
	contU = contour.MinU; \
	contV = contour.MinV; \
	dxU = (contour.MaxU - contU) / contLen; \
	dxV = (contour.MaxV - contV) / contLen

#define SCANLINE_STEP_Z() contZ += dxZ
#define SCANLINE_STEP_RGB() \
	contR += dxR; \
	contG += dxG; \
	contB += dxB
#define SCANLINE_STEP_UV() \
	contU += dxU; \
	contV += dxV

#define SCANLINE_STEP_Z_BY(diff) contZ += dxZ * diff
#define SCANLINE_STEP_RGB_BY(diff) \
	contR += dxR * diff; \
	contG += dxG * diff; \
	contB += dxB * diff
#define SCANLINE_STEP_UV_BY(diff) \
	contU += dxU * diff; \
	contV += dxV * diff

#define SCANLINE_GET_MAPZ() mapZ = 1.0f / contZ
#define SCANLINE_GET_INVZ() \
	Uint32 iz = mapZ * 65536; \
	(void)iz /* Make compiler shut up */
#define SCANLINE_GET_RGB() \
	Sint32 colR = contR; \
	Sint32 colG = contG; \
	Sint32 colB = contB
#define SCANLINE_GET_RGB_PERSP() \
	Sint32 colR = contR * mapZ; \
	Sint32 colG = contG * mapZ; \
	Sint32 colB = contB * mapZ
#define SCANLINE_GET_MAPUV() \
	float mapU = contU; \
	float mapV = contV
#define SCANLINE_GET_TEXUV() \
	int texU = ((int)((mapU + 1) * texture->Width) >> 16) % texture->Width; \
	int texV = ((int)((mapV + 1) * texture->Height) >> 16) % texture->Height
#define SCANLINE_GET_COLOR() \
	CLAMP_VAL(colR, 0x00, 0xFF0000); \
	CLAMP_VAL(colG, 0x00, 0xFF0000); \
	CLAMP_VAL(colB, 0x00, 0xFF0000); \
	col = 0xFF000000U | ((colR) & 0xFF0000) | ((colG >> 8) & 0xFF00) | ((colB >> 16) & 0xFF)

#define SCANLINE_WRITE_PIXEL(px) \
	pixelFunction((Uint32*)&px, \
		&dstPx[dst_x + dst_strideY], \
		blendState, \
		multTableAt, \
		multSubTableAt)

Contour* ContourField = SoftwareRenderer::ContourBuffer;

template<typename T>
static void GetPolygonBounds(T* positions, int count, int& minVal, int& maxVal) {
	minVal = INT_MAX;
	maxVal = INT_MIN;

	while (count--) {
		int tempY = positions->Y >> 16;
		if (minVal > tempY) {
			minVal = tempY;
		}
		if (maxVal < tempY) {
			maxVal = tempY;
		}
		positions++;
	}
}

#define CLIP_BOUNDS() \
	if (Graphics::CurrentClip.Enabled) { \
		if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height) \
			dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height; \
		if (dst_y1 < Graphics::CurrentClip.Y) \
			dst_y1 = Graphics::CurrentClip.Y; \
	} \
	if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height) \
		dst_y2 = (int)Graphics::CurrentRenderTarget->Height; \
	if (dst_y1 < 0) \
		dst_y1 = 0; \
	if (dst_y2 < 0 || dst_y1 >= dst_y2) \
	return

// Draws a polygon
void PolygonRasterizer::DrawBasic(Vector2* positions,
	Uint32 color,
	int count,
	BlendState blendState) {
	if (!Graphics::CurrentRenderTarget) {
		return;
	}

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	int dst_y1, dst_y2;

	if (count == 0) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;
	if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT) {
		return;
	}

	GetPolygonBounds<Vector2>(positions, count, dst_y1, dst_y2);
	CLIP_BOUNDS();

	Scanline::Prepare(dst_y1, dst_y2);

	Vector2* lastVector = positions;
	if (count > 1) {
		int countRem = count - 1;
		while (countRem--) {
			Scanline::Process(
				lastVector[0].X, lastVector[0].Y, lastVector[1].X, lastVector[1].Y);
			lastVector++;
		}
	}
	Scanline::Process(lastVector[0].X, lastVector[0].Y, positions[0].X, positions[0].Y);

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;
	if (!SoftwareRenderer::IsStencilEnabled() &&
		((blendFlag & (BlendFlag_MODE_MASK | BlendFlag_TINT_BIT)) == BlendFlag_OPAQUE)) {
		for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
			Contour contour = ContourField[dst_y];
			if (contour.MaxX < contour.MinX) {
				dst_strideY += dstStride;
				continue;
			}

			Memory::Memset4(&dstPx[contour.MinX + dst_strideY],
				color,
				contour.MaxX - contour.MinX);
			dst_strideY += dstStride;
		}
	}
	else {
		for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
			Contour contour = ContourField[dst_y];
			if (contour.MaxX < contour.MinX) {
				dst_strideY += dstStride;
				continue;
			}
			for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) {
				SCANLINE_WRITE_PIXEL(color);
			}
			dst_strideY += dstStride;
		}
	}
}
// Draws a blended polygon
void PolygonRasterizer::DrawBasicBlend(Vector2* positions,
	int* colors,
	int count,
	BlendState blendState) {
	if (!Graphics::CurrentRenderTarget) {
		return;
	}

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	int dst_y1, dst_y2;

	if (count == 0) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;
	if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT) {
		return;
	}

	GetPolygonBounds<Vector2>(positions, count, dst_y1, dst_y2);
	CLIP_BOUNDS();

	Scanline::Prepare(dst_y1, dst_y2);

	int* lastColor = colors;
	Vector2* lastVector = positions;
	if (count > 1) {
		int countRem = count - 1;
		while (countRem--) {
			Scanline::Process(lastColor[0],
				lastColor[1],
				lastVector[0].X,
				lastVector[0].Y,
				lastVector[1].X,
				lastVector[1].Y);
			lastVector++;
			lastColor++;
		}
	}

	Scanline::Process(lastColor[0],
		colors[0],
		lastVector[0].X,
		lastVector[0].Y,
		positions[0].X,
		positions[0].Y);

	Sint32 col, contLen;
	float contR, contG, contB, dxR, dxG, dxB;

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
		Contour contour = ContourField[dst_y];
		contLen = contour.MaxX - contour.MinX;
		if (contLen <= 0) {
			dst_strideY += dstStride;
			continue;
		}
		SCANLINE_INIT_RGB();
		if (contour.MapLeft < contour.MinX) {
			int diff = contour.MinX - contour.MapLeft;
			SCANLINE_STEP_RGB_BY(diff);
		}
		for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) {
			SCANLINE_GET_RGB();
			SCANLINE_GET_COLOR();
			SCANLINE_WRITE_PIXEL(col);
			SCANLINE_STEP_RGB();
		}
		dst_strideY += dstStride;
	}
}
// Draws a polygon with lighting
void PolygonRasterizer::DrawShaded(Vector3* positions,
	Uint32 color,
	int count,
	BlendState blendState) {
	if (!Graphics::CurrentRenderTarget) {
		return;
	}

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	int dst_y1, dst_y2;

	if (count == 0) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;
	if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT) {
		return;
	}

	GetPolygonBounds<Vector3>(positions, count, dst_y1, dst_y2);
	CLIP_BOUNDS();

	Scanline::Prepare(dst_y1, dst_y2);

	Vector3* lastVector = positions;
	if (count > 1) {
		int countRem = count - 1;
		while (countRem--) {
			Scanline::ProcessDepth(lastVector[0].X,
				lastVector[0].Y,
				lastVector[0].Z,
				lastVector[1].X,
				lastVector[1].Y,
				lastVector[1].Z);
			lastVector++;
		}
	}

	Scanline::ProcessDepth(lastVector[0].X,
		lastVector[0].Y,
		lastVector[0].Z,
		positions[0].X,
		positions[0].Y,
		positions[0].Z);

	Sint32 col, contLen;
	float contZ, dxZ;
	float mapZ;

	color |= 0xFF000000U;

#define PX_GET() SCANLINE_WRITE_PIXEL(color)
#define PX_GET_FOG() \
	col = DoFogLighting(color, mapZ); \
	SCANLINE_WRITE_PIXEL(col)

#define DRAW_POLYGONSHADED(pixelRead) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		Contour contour = ContourField[dst_y]; \
		contLen = contour.MaxX - contour.MinX; \
		if (contLen <= 0) { \
			dst_strideY += dstStride; \
			continue; \
		} \
		for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
			pixelRead(); \
		} \
		dst_strideY += dstStride; \
	}
#define DRAW_POLYGONSHADED_FOG(pixelRead) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		Contour contour = ContourField[dst_y]; \
		contLen = contour.MaxX - contour.MinX; \
		if (contLen <= 0) { \
			dst_strideY += dstStride; \
			continue; \
		} \
		SCANLINE_INIT_Z(); \
		if (contour.MapLeft < contour.MinX) { \
			SCANLINE_STEP_Z_BY(contour.MinX - contour.MapLeft); \
		} \
		for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
			SCANLINE_GET_MAPZ(); \
			pixelRead(); \
			SCANLINE_STEP_Z(); \
		} \
		dst_strideY += dstStride; \
	}

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;

	if (UseFog) {
		POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONSHADED_FOG, PX_GET_FOG);
	}
	else {
		POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONSHADED, PX_GET);
	}

#undef PX_GET
#undef PX_GET_FOG

#undef DRAW_POLYGONSHADED
#undef DRAW_POLYGONSHADED_FOG
}
// Draws a blended polygon with lighting
void PolygonRasterizer::DrawBlendShaded(Vector3* positions,
	int* colors,
	int count,
	BlendState blendState) {
	if (!Graphics::CurrentRenderTarget) {
		return;
	}

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	int dst_y1, dst_y2;

	if (count == 0) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;
	if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT) {
		return;
	}

	GetPolygonBounds<Vector3>(positions, count, dst_y1, dst_y2);
	CLIP_BOUNDS();

	Scanline::Prepare(dst_y1, dst_y2);

	int* lastColor = colors;
	Vector3* lastVector = positions;
	if (count > 1) {
		int countRem = count - 1;
		while (countRem--) {
			Scanline::ProcessDepth(lastColor[0],
				lastColor[1],
				lastVector[0].X,
				lastVector[0].Y,
				lastVector[0].Z,
				lastVector[1].X,
				lastVector[1].Y,
				lastVector[1].Z);
			lastVector++;
			lastColor++;
		}
	}

	Scanline::ProcessDepth(lastColor[0],
		colors[0],
		lastVector[0].X,
		lastVector[0].Y,
		lastVector[0].Z,
		positions[0].X,
		positions[0].Y,
		positions[0].Z);

	Sint32 col, contLen;
	float contZ, contR, contG, contB;
	float dxZ, dxR, dxG, dxB;
	float mapZ;

#define PX_GET() \
	SCANLINE_GET_COLOR(); \
	SCANLINE_WRITE_PIXEL(col)
#define PX_GET_FOG() \
	SCANLINE_GET_COLOR(); \
	col = DoFogLighting(col, mapZ); \
	SCANLINE_WRITE_PIXEL(col)

#define DRAW_POLYGONBLENDSHADED(pixelRead) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		Contour contour = ContourField[dst_y]; \
		contLen = contour.MaxX - contour.MinX; \
		if (contLen <= 0) { \
			dst_strideY += dstStride; \
			continue; \
		} \
		SCANLINE_INIT_Z(); \
		SCANLINE_INIT_RGB(); \
		if (contour.MapLeft < contour.MinX) { \
			int diff = contour.MinX - contour.MapLeft; \
			SCANLINE_STEP_Z_BY(diff); \
			SCANLINE_STEP_RGB_BY(diff); \
		} \
		for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
			SCANLINE_GET_MAPZ(); \
			SCANLINE_GET_RGB(); \
			pixelRead(); \
			SCANLINE_STEP_Z(); \
			SCANLINE_STEP_RGB(); \
		} \
		dst_strideY += dstStride; \
	}

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;

	if (UseFog) {
		POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONBLENDSHADED, PX_GET_FOG);
	}
	else {
		POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONBLENDSHADED, PX_GET);
	}

#undef PX_GET
#undef PX_GET_FOG

#undef DRAW_POLYGONBLENDSHADED
}
// Draws an affine texture mapped polygon
void PolygonRasterizer::DrawAffine(Texture* texture,
	Vector3* positions,
	Vector2* uvs,
	Uint32 color,
	int count,
	BlendState blendState) {
	if (!Graphics::CurrentRenderTarget) {
		return;
	}

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;
	Uint32* srcPx = (Uint32*)texture->Pixels;
	Uint32 srcStride = texture->Width;

	int dst_y1, dst_y2;

	if (count == 0) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;
	if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT) {
		return;
	}

	GetPolygonBounds<Vector3>(positions, count, dst_y1, dst_y2);
	CLIP_BOUNDS();

	Scanline::Prepare(dst_y1, dst_y2);

	Vector3* lastVector = positions;
	Vector2* lastUV = uvs;
	if (count > 1) {
		int countRem = count - 1;
		while (countRem--) {
			Scanline::ProcessUVAffine(lastUV[0],
				lastUV[1],
				lastVector[0].X,
				lastVector[0].Y,
				lastVector[0].Z,
				lastVector[1].X,
				lastVector[1].Y,
				lastVector[1].Z);
			lastVector++;
			lastUV++;
		}
	}

	Scanline::ProcessUVAffine(lastUV[0],
		uvs[0],
		lastVector[0].X,
		lastVector[0].Y,
		lastVector[0].Z,
		positions[0].X,
		positions[0].Y,
		positions[0].Z);

	Sint32 col, texCol, contLen;
	float contZ, contU, contV;
	float dxZ, dxU, dxV;
	float mapZ;

	int min_x, max_x;
	if (Graphics::CurrentClip.Enabled) {
		min_x = Graphics::CurrentClip.X;
		max_x = Graphics::CurrentClip.X + Graphics::CurrentClip.Width;
	}
	else {
		min_x = 0;
		max_x = (int)Graphics::CurrentRenderTarget->Width;
	}

#define DRAW_PLACEPIXEL(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
		col = DoColorTint(color, texCol); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_PAL(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) && (index[texCol] & 0xFF000000U)) { \
		col = DoColorTint(color, index[texCol]); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_FOG(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
		col = DoColorTint(color, texCol); \
		col = DoFogLighting(col, mapZ); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_PAL_FOG(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) && (index[texCol] & 0xFF000000U)) { \
		col = DoColorTint(color, index[texCol]); \
		col = DoFogLighting(col, mapZ); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_POLYGONAFFINE(placePixelMacro, dpR, dpW) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		Contour contour = ContourField[dst_y]; \
		contLen = contour.MaxX - contour.MinX; \
		if (contLen <= 0) { \
			dst_strideY += dstStride; \
			continue; \
		} \
		SCANLINE_INIT_Z(); \
		SCANLINE_INIT_UV(); \
		if (contour.MinX < min_x) { \
			int diff = min_x - contour.MinX; \
			SCANLINE_STEP_Z_BY(diff); \
			SCANLINE_STEP_UV_BY(diff); \
			contour.MinX = min_x; \
		} \
		if (contour.MaxX > max_x) \
			contour.MaxX = max_x; \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		else \
			index = &Graphics::PaletteColors[0][0]; \
		for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
			SCANLINE_GET_MAPZ(); \
			SCANLINE_GET_INVZ(); \
			if (dpR(iz)) { \
				SCANLINE_GET_MAPUV(); \
				SCANLINE_GET_TEXUV(); \
				placePixelMacro(dpW); \
			} \
			SCANLINE_STEP_Z(); \
			SCANLINE_STEP_UV(); \
		} \
		dst_strideY += dstStride; \
	}

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

	Uint32* index;
	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;

	POLYGON_SCANLINE_DEPTH(DRAW_POLYGONAFFINE);

#undef DRAW_PLACEPIXEL
#undef DRAW_PLACEPIXEL_PAL
#undef DRAW_PLACEPIXEL_FOG
#undef DRAW_PLACEPIXEL_PAL_FOG
#undef DRAW_POLYGONAFFINE
}
// Draws an affine texture mapped polygon with blending
void PolygonRasterizer::DrawBlendAffine(Texture* texture,
	Vector3* positions,
	Vector2* uvs,
	int* colors,
	int count,
	BlendState blendState) {
	if (!Graphics::CurrentRenderTarget) {
		return;
	}

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;
	Uint32* srcPx = (Uint32*)texture->Pixels;
	Uint32 srcStride = texture->Width;

	int dst_y1, dst_y2;

	if (count == 0) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;
	if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT) {
		return;
	}

	GetPolygonBounds<Vector3>(positions, count, dst_y1, dst_y2);
	CLIP_BOUNDS();

	Scanline::Prepare(dst_y1, dst_y2);

	int* lastColor = colors;
	Vector3* lastPosition = positions;
	Vector2* lastUV = uvs;
	if (count > 1) {
		int countRem = count - 1;
		while (countRem--) {
			Scanline::ProcessUVAffine(lastColor[0],
				lastColor[1],
				lastUV[0],
				lastUV[1],
				lastPosition[0].X,
				lastPosition[0].Y,
				lastPosition[0].Z,
				lastPosition[1].X,
				lastPosition[1].Y,
				lastPosition[1].Z);
			lastPosition++;
			lastColor++;
			lastUV++;
		}
	}

	Scanline::ProcessUVAffine(lastColor[0],
		colors[0],
		lastUV[0],
		uvs[0],
		lastPosition[0].X,
		lastPosition[0].Y,
		lastPosition[0].Z,
		positions[0].X,
		positions[0].Y,
		positions[0].Z);

	Sint32 col, texCol, contLen;
	float contZ, contU, contV, contR, contG, contB;
	float dxZ, dxU, dxV, dxR, dxG, dxB;
	float mapZ;

	int min_x, max_x;
	if (Graphics::CurrentClip.Enabled) {
		min_x = Graphics::CurrentClip.X;
		max_x = Graphics::CurrentClip.X + Graphics::CurrentClip.Width;
	}
	else {
		min_x = 0;
		max_x = (int)Graphics::CurrentRenderTarget->Width;
	}

#define DRAW_PLACEPIXEL(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
		SCANLINE_GET_COLOR(); \
		col = DoColorTint(col, texCol); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_PAL(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) && (index[texCol] & 0xFF000000U)) { \
		SCANLINE_GET_COLOR(); \
		col = DoColorTint(col, index[texCol]); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_FOG(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
		SCANLINE_GET_COLOR(); \
		col = DoColorTint(col, texCol); \
		col = DoFogLighting(col, mapZ); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_PAL_FOG(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) && (index[texCol] & 0xFF000000U)) { \
		SCANLINE_GET_COLOR(); \
		col = DoColorTint(col, index[texCol]); \
		col = DoFogLighting(col, mapZ); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_POLYGONBLENDAFFINE(placePixelMacro, dpR, dpW) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		Contour contour = ContourField[dst_y]; \
		contLen = contour.MaxX - contour.MinX; \
		if (contLen <= 0) { \
			dst_strideY += dstStride; \
			continue; \
		} \
		SCANLINE_INIT_Z(); \
		SCANLINE_INIT_UV(); \
		SCANLINE_INIT_RGB(); \
		if (contour.MinX < min_x) { \
			int diff = min_x - contour.MinX; \
			SCANLINE_STEP_Z_BY(diff); \
			SCANLINE_STEP_UV_BY(diff); \
			SCANLINE_STEP_RGB_BY(diff); \
			contour.MinX = min_x; \
		} \
		if (contour.MaxX > max_x) \
			contour.MaxX = max_x; \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		else \
			index = &Graphics::PaletteColors[0][0]; \
		for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
			SCANLINE_GET_MAPZ(); \
			SCANLINE_GET_INVZ(); \
			if (dpR(iz)) { \
				SCANLINE_GET_RGB(); \
				SCANLINE_GET_MAPUV(); \
				SCANLINE_GET_TEXUV(); \
				placePixelMacro(dpW); \
			} \
			SCANLINE_STEP_Z(); \
			SCANLINE_STEP_UV(); \
			SCANLINE_STEP_RGB(); \
		} \
		dst_strideY += dstStride; \
	}

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

	Uint32* index;
	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;

	POLYGON_SCANLINE_DEPTH(DRAW_POLYGONBLENDAFFINE);

#undef DRAW_PLACEPIXEL
#undef DRAW_PLACEPIXEL_PAL
#undef DRAW_PLACEPIXEL_FOG
#undef DRAW_PLACEPIXEL_PAL_FOG
#undef DRAW_POLYGONBLENDAFFINE
#undef BLENDFLAGS
}
// Draws a perspective-correct texture mapped polygon
#ifdef SCANLINE_DIVISION
#define PERSP_MAPPER_SUBDIVIDE(width, placePixelMacro, dpR, dpW) \
	contZ += dxZ * width; \
	contU += dxU * width; \
	contV += dxV * width; \
	endZ = 1.0f / contZ; \
	endU = contU * endZ; \
	endV = contV * endZ; \
	stepZ = (endZ - startZ) * invContSize; \
	stepU = (endU - startU) * invContSize; \
	stepV = (endV - startV) * invContSize; \
	mapZ = startZ; \
	currU = startU; \
	currV = startV; \
	for (int i = 0; i < width; i++) { \
		Uint32 iz = mapZ * 65536; \
		if (dpR(iz)) { \
			DRAW_PERSP_GET(currU, currV); \
			placePixelMacro(dpW); \
		} \
		DRAW_PERSP_STEP(); \
		mapZ += stepZ; \
		currU += stepU; \
		currV += stepV; \
		dst_x++; \
	}
#define DO_PERSP_MAPPING(placePixelMacro, dpR, dpW) \
	{ \
		int dst_x = contour.MinX; \
		int contWidth = contour.MaxX - contour.MinX; \
		int contSize = 16; \
		float invContSize = 1.0f / contSize; \
		float startZ = 1.0f / contZ; \
		float startU = contU * startZ; \
		float startV = contV * startZ; \
		float endZ, endU, endV; \
		float stepZ, stepU, stepV; \
		float mapZ, currU, currV; \
		while (contWidth >= contSize) { \
			PERSP_MAPPER_SUBDIVIDE(contSize, placePixelMacro, dpR, dpW); \
			startU = endU; \
			startV = endV; \
			startZ = endZ; \
			contWidth -= contSize; \
		} \
		if (contWidth > 0) { \
			if (contWidth > 1) { \
				PERSP_MAPPER_SUBDIVIDE(contWidth, placePixelMacro, dpR, dpW); \
			} \
			else { \
				Uint32 iz = startZ * 65536; \
				if (dpR(iz)) { \
					DRAW_PERSP_GET(startU, startV); \
					placePixelMacro(dpW); \
				} \
			} \
		} \
	}
#else
#define DO_PERSP_MAPPING(placePixelMacro, dpR, dpW) \
	for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
		SCANLINE_GET_MAPZ(); \
		SCANLINE_GET_INVZ(); \
		if (dpR(iz)) { \
			DRAW_PERSP_GET(contU* mapZ, contV* mapZ); \
			placePixelMacro(dpW); \
		} \
		SCANLINE_STEP_Z(); \
		SCANLINE_STEP_UV(); \
		DRAW_PERSP_STEP(); \
	}
#endif
void PolygonRasterizer::DrawPerspective(Texture* texture,
	Vector3* positions,
	Vector2* uvs,
	Uint32 color,
	int count,
	BlendState blendState) {
	if (!Graphics::CurrentRenderTarget) {
		return;
	}

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;
	Uint32* srcPx = (Uint32*)texture->Pixels;
	Uint32 srcStride = texture->Width;

	int dst_y1, dst_y2;

	if (count == 0) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;
	if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT) {
		return;
	}

	GetPolygonBounds<Vector3>(positions, count, dst_y1, dst_y2);
	CLIP_BOUNDS();

	Scanline::Prepare(dst_y1, dst_y2);

	Vector3* lastVector = positions;
	Vector2* lastUV = uvs;
	if (count > 1) {
		int countRem = count - 1;
		while (countRem--) {
			Scanline::ProcessUV(lastUV[0],
				lastUV[1],
				lastVector[0].X,
				lastVector[0].Y,
				lastVector[0].Z,
				lastVector[1].X,
				lastVector[1].Y,
				lastVector[1].Z);
			lastVector++;
			lastUV++;
		}
	}

	Scanline::ProcessUV(lastUV[0],
		uvs[0],
		lastVector[0].X,
		lastVector[0].Y,
		lastVector[0].Z,
		positions[0].X,
		positions[0].Y,
		positions[0].Z);

	Sint32 col, texCol, contLen;
	float contZ, contU, contV;
	float dxZ, dxU, dxV;
	float mapZ;

#define DRAW_PLACEPIXEL(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
		col = DoColorTint(color, texCol); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_PAL(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) && (index[texCol] & 0xFF000000U)) { \
		col = DoColorTint(color, index[texCol]); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_FOG(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
		col = DoColorTint(color, texCol); \
		col = DoFogLighting(col, mapZ); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_PAL_FOG(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) && (index[texCol] & 0xFF000000U)) { \
		col = DoColorTint(color, index[texCol]); \
		col = DoFogLighting(col, mapZ); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PERSP_GET(u, v) \
	float mapU = u; \
	float mapV = v; \
	SCANLINE_GET_TEXUV()

#define DRAW_PERSP_STEP()

#define DRAW_POLYGONPERSP(placePixelMacro, dpR, dpW) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		Contour contour = ContourField[dst_y]; \
		contLen = contour.MapRight - contour.MapLeft; \
		if (contLen <= 0) { \
			dst_strideY += dstStride; \
			continue; \
		} \
		SCANLINE_INIT_Z(); \
		SCANLINE_INIT_UV(); \
		if (contour.MapLeft < contour.MinX) { \
			int diff = contour.MinX - contour.MapLeft; \
			SCANLINE_STEP_Z_BY(diff); \
			SCANLINE_STEP_UV_BY(diff); \
		} \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		else \
			index = &Graphics::PaletteColors[0][0]; \
		DO_PERSP_MAPPING(placePixelMacro, dpR, dpW); \
		dst_strideY += dstStride; \
	}

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

	Uint32* index;
	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;

	POLYGON_SCANLINE_DEPTH(DRAW_POLYGONPERSP);

#undef DRAW_PLACEPIXEL
#undef DRAW_PLACEPIXEL_PAL
#undef DRAW_PLACEPIXEL_FOG
#undef DRAW_PLACEPIXEL_PAL_FOG
#undef DRAW_PERSP_GET
#undef DRAW_PERSP_STEP
#undef DRAW_POLYGONPERSP
}
// Draws a perspective-correct texture mapped polygon with blending
void PolygonRasterizer::DrawBlendPerspective(Texture* texture,
	Vector3* positions,
	Vector2* uvs,
	int* colors,
	int count,
	BlendState blendState) {
	if (!Graphics::CurrentRenderTarget) {
		return;
	}

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;
	Uint32* srcPx = (Uint32*)texture->Pixels;
	Uint32 srcStride = texture->Width;

	int dst_y1, dst_y2;

	if (count == 0) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;
	if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT) {
		return;
	}

	GetPolygonBounds<Vector3>(positions, count, dst_y1, dst_y2);
	CLIP_BOUNDS();

	Scanline::Prepare(dst_y1, dst_y2);

	int* lastColor = colors;
	Vector3* lastPosition = positions;
	Vector2* lastUV = uvs;
	if (count > 1) {
		int countRem = count - 1;
		while (countRem--) {
			Scanline::ProcessUV(lastColor[0],
				lastColor[1],
				lastUV[0],
				lastUV[1],
				lastPosition[0].X,
				lastPosition[0].Y,
				lastPosition[0].Z,
				lastPosition[1].X,
				lastPosition[1].Y,
				lastPosition[1].Z);
			lastPosition++;
			lastColor++;
			lastUV++;
		}
	}

	Scanline::ProcessUV(lastColor[0],
		colors[0],
		lastUV[0],
		uvs[0],
		lastPosition[0].X,
		lastPosition[0].Y,
		lastPosition[0].Z,
		positions[0].X,
		positions[0].Y,
		positions[0].Z);

	Sint32 col, texCol, contLen;
	float contZ, contU, contV, contR, contG, contB;
	float dxZ, dxU, dxV, dxR, dxG, dxB;
	float mapZ;

#define DRAW_PLACEPIXEL(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
		SCANLINE_GET_COLOR(); \
		col = DoColorTint(col, texCol); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_PAL(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) && (index[texCol] & 0xFF000000U)) { \
		SCANLINE_GET_COLOR(); \
		col = DoColorTint(col, index[texCol]); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_FOG(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
		SCANLINE_GET_COLOR(); \
		col = DoColorTint(col, texCol); \
		col = DoFogLighting(col, mapZ); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PLACEPIXEL_PAL_FOG(dpW) \
	if ((texCol = srcPx[(texV * srcStride) + texU]) && (index[texCol] & 0xFF000000U)) { \
		SCANLINE_GET_COLOR(); \
		col = DoColorTint(col, index[texCol]); \
		col = DoFogLighting(col, mapZ); \
		SCANLINE_WRITE_PIXEL(col); \
		dpW(iz); \
	}

#define DRAW_PERSP_GET(u, v) \
	float mapU = u; \
	float mapV = v; \
	SCANLINE_GET_RGB_PERSP(); \
	SCANLINE_GET_TEXUV()

#define DRAW_PERSP_STEP() SCANLINE_STEP_RGB()

#define DRAW_POLYGONBLENDPERSP(placePixelMacro, dpR, dpW) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		Contour contour = ContourField[dst_y]; \
		contLen = contour.MapRight - contour.MapLeft; \
		if (contLen <= 0) { \
			dst_strideY += dstStride; \
			continue; \
		} \
		SCANLINE_INIT_Z(); \
		SCANLINE_INIT_RGB(); \
		SCANLINE_INIT_UV(); \
		if (contour.MapLeft < contour.MinX) { \
			int diff = contour.MinX - contour.MapLeft; \
			SCANLINE_STEP_Z_BY(diff); \
			SCANLINE_STEP_RGB_BY(diff); \
			SCANLINE_STEP_UV_BY(diff); \
		} \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		else \
			index = &Graphics::PaletteColors[0][0]; \
		DO_PERSP_MAPPING(placePixelMacro, dpR, dpW); \
		dst_strideY += dstStride; \
	}

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

	Uint32* index;
	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;

	POLYGON_SCANLINE_DEPTH(DRAW_POLYGONBLENDPERSP);

#undef DRAW_PLACEPIXEL
#undef DRAW_PLACEPIXEL_PAL
#undef DRAW_PLACEPIXEL_FOG
#undef DRAW_PLACEPIXEL_PAL_FOG
#undef DRAW_PERSP_GET
#undef DRAW_PERSP_STEP
#undef DRAW_POLYGONBLENDPERSP
}
// Draws a polygon with depth testing
void PolygonRasterizer::DrawDepth(Vector3* positions,
	Uint32 color,
	int count,
	BlendState blendState) {
	if (!Graphics::CurrentRenderTarget) {
		return;
	}

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	int dst_y1, dst_y2;

	if (count == 0) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;
	if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT) {
		return;
	}

	GetPolygonBounds<Vector3>(positions, count, dst_y1, dst_y2);
	CLIP_BOUNDS();

	Scanline::Prepare(dst_y1, dst_y2);

	Vector3* lastVector = positions;
	if (count > 1) {
		int countRem = count - 1;
		while (countRem--) {
			Scanline::ProcessDepth(lastVector[0].X,
				lastVector[0].Y,
				lastVector[0].Z,
				lastVector[1].X,
				lastVector[1].Y,
				lastVector[1].Z);
			lastVector++;
		}
	}

	Scanline::ProcessDepth(lastVector[0].X,
		lastVector[0].Y,
		lastVector[0].Z,
		positions[0].X,
		positions[0].Y,
		positions[0].Z);

	Sint32 col, contLen;
	float contZ, dxZ;
	float mapZ;

	color |= 0xFF000000U;

#define PX_GET() \
	SCANLINE_GET_MAPZ(); \
	SCANLINE_GET_INVZ(); \
	if (DEPTH_READ_U32(iz)) { \
		SCANLINE_WRITE_PIXEL(color); \
		DEPTH_WRITE_U32(iz); \
	}
#define PX_GET_FOG() \
	SCANLINE_GET_MAPZ(); \
	SCANLINE_GET_INVZ(); \
	if (DEPTH_READ_U32(iz)) { \
		col = DoFogLighting(color, mapZ); \
		SCANLINE_WRITE_PIXEL(col); \
		DEPTH_WRITE_U32(iz); \
	}

#define DRAW_POLYGONDEPTH(pixelRead) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		Contour contour = ContourField[dst_y]; \
		contLen = contour.MaxX - contour.MinX; \
		if (contLen <= 0) { \
			dst_strideY += dstStride; \
			continue; \
		} \
		SCANLINE_INIT_Z(); \
		if (contour.MapLeft < contour.MinX) { \
			SCANLINE_STEP_Z_BY(contour.MinX - contour.MapLeft); \
		} \
		for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
			pixelRead(); \
			SCANLINE_STEP_Z(); \
		} \
		dst_strideY += dstStride; \
	}

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;

	if (UseFog) {
		POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONDEPTH, PX_GET_FOG);
	}
	else {
		POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONDEPTH, PX_GET);
	}

#undef PX_GET
#undef PX_GET_FOG

#undef DRAW_POLYGONDEPTH
}
// Draws a blended polygon with depth testing
void PolygonRasterizer::DrawBlendDepth(Vector3* positions,
	int* colors,
	int count,
	BlendState blendState) {
	if (!Graphics::CurrentRenderTarget) {
		return;
	}

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	int dst_y1, dst_y2;

	if (count == 0) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;
	if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT) {
		return;
	}

	GetPolygonBounds<Vector3>(positions, count, dst_y1, dst_y2);
	CLIP_BOUNDS();

	Scanline::Prepare(dst_y1, dst_y2);

	int* lastColor = colors;
	Vector3* lastVector = positions;
	if (count > 1) {
		int countRem = count - 1;
		while (countRem--) {
			Scanline::ProcessDepth(lastColor[0],
				lastColor[1],
				lastVector[0].X,
				lastVector[0].Y,
				lastVector[0].Z,
				lastVector[1].X,
				lastVector[1].Y,
				lastVector[1].Z);
			lastVector++;
			lastColor++;
		}
	}

	Scanline::ProcessDepth(lastColor[0],
		colors[0],
		lastVector[0].X,
		lastVector[0].Y,
		lastVector[0].Z,
		positions[0].X,
		positions[0].Y,
		positions[0].Z);

	Sint32 col, contLen;
	float contZ, contR, contG, contB;
	float dxZ, dxR, dxG, dxB;
	float mapZ;

#define PX_GET() \
	SCANLINE_GET_MAPZ(); \
	SCANLINE_GET_INVZ(); \
	if (DEPTH_READ_U32(iz)) { \
		SCANLINE_GET_RGB(); \
		SCANLINE_GET_COLOR(); \
		SCANLINE_WRITE_PIXEL(col); \
		DEPTH_WRITE_U32(iz); \
	}
#define PX_GET_FOG() \
	SCANLINE_GET_MAPZ(); \
	SCANLINE_GET_INVZ(); \
	if (DEPTH_READ_U32(iz)) { \
		SCANLINE_GET_RGB(); \
		SCANLINE_GET_COLOR(); \
		col = DoFogLighting(col, mapZ); \
		SCANLINE_WRITE_PIXEL(col); \
		DEPTH_WRITE_U32(iz); \
	}

#define DRAW_POLYGONBLENDDEPTH(pixelRead) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		Contour contour = ContourField[dst_y]; \
		contLen = contour.MaxX - contour.MinX; \
		if (contLen <= 0) { \
			dst_strideY += dstStride; \
			continue; \
		} \
		SCANLINE_INIT_Z(); \
		SCANLINE_INIT_RGB(); \
		if (contour.MapLeft < contour.MinX) { \
			int diff = contour.MinX - contour.MapLeft; \
			SCANLINE_STEP_Z_BY(diff); \
			SCANLINE_STEP_RGB_BY(diff); \
		} \
		for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
			pixelRead(); \
			SCANLINE_STEP_Z(); \
			SCANLINE_STEP_RGB(); \
		} \
		dst_strideY += dstStride; \
	}

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;

	if (UseFog) {
		POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONBLENDDEPTH, PX_GET_FOG);
	}
	else {
		POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONBLENDDEPTH, PX_GET);
	}

#undef PX_GET
#undef PX_GET_FOG

#undef DRAW_POLYGONBLENDDEPTH
}

void PolygonRasterizer::SetDepthTest(bool enabled) {
	DepthTest = enabled;
	if (!DepthTest) {
		return;
	}

	size_t dpSize =
		Graphics::CurrentRenderTarget->Width * Graphics::CurrentRenderTarget->Height;
	if (DepthBuffer == NULL || dpSize > DepthBufferSize) {
		DepthBufferSize = dpSize;
		DepthBuffer = (Uint32*)Memory::Realloc(
			DepthBuffer, DepthBufferSize * sizeof(*DepthBuffer));
	}

	memset(DepthBuffer, 0xFF, dpSize * sizeof(*DepthBuffer));
}
void PolygonRasterizer::FreeDepthBuffer(void) {
	Memory::Free(DepthBuffer);
	DepthBuffer = NULL;
}

void PolygonRasterizer::SetUseDepthBuffer(bool enabled) {
	UseDepthBuffer = enabled;
}

void PolygonRasterizer::SetUseFog(bool enabled) {
	UseFog = enabled;
}
void PolygonRasterizer::SetFogEquation(FogEquation equation) {
	switch (equation) {
	case FogEquation_Exp:
		FogEquationFunc = FogEquationFunc_Exp;
		break;
	default:
		FogEquationFunc = FogEquationFunc_Linear;
		break;
	}
}
void PolygonRasterizer::SetFogStart(float start) {
	FogStart = start;
}
void PolygonRasterizer::SetFogEnd(float end) {
	FogEnd = end;
}
void PolygonRasterizer::SetFogDensity(float density) {
	FogDensity = density;
}
void PolygonRasterizer::SetFogColor(float r, float g, float b) {
	Uint8 colorR = (Uint32)(r * 0xFF);
	Uint8 colorG = (Uint32)(g * 0xFF);
	Uint8 colorB = (Uint32)(b * 0xFF);
	Uint32 result = colorR << 16 | colorG << 8 | colorB;
	Graphics::ConvertFromARGBtoNative(&result, 1);
	FogColor = result;
}
void PolygonRasterizer::SetFogSmoothness(float smoothness) {
	float value = Math::Clamp(1.0f - smoothness, 0.0f, 1.0f);
	if (value <= 0.0) {
		for (size_t i = 0; i <= 0x100; i++) {
			FogTable[i] = i;
		}
		return;
	}

	float fog = 0.0f;
	float inv = 1.0f / value;

	const float recip = 1.0f / 256.0f;

	for (size_t i = 0; i <= 0x100; i++) {
		FogTable[i] = (int)(floor(fog) * value * 256.0f);
		fog += recip * inv;
	}
}
