#include <Engine/Rendering/FaceInfo.h>
#include <Engine/Rendering/ModelRenderer.h>
#include <Engine/Rendering/PolygonRenderer.h>
#include <Engine/Rendering/Scene3D.h>
#include <Engine/Rendering/Software/PolygonRasterizer.h>
#include <Engine/Rendering/Software/SoftwareEnums.h>
#include <Engine/Rendering/Software/SoftwareRenderer.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/ResourceStream.h>

#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/Types.h>

GraphicsFunctions SoftwareRenderer::BackendFunctions;
Uint32 SoftwareRenderer::CompareColor = 0xFF000000U;
TileScanLine SoftwareRenderer::TileScanLineBuffer[MAX_FRAMEBUFFER_HEIGHT];
Sint32 SoftwareRenderer::SpriteDeformBuffer[MAX_FRAMEBUFFER_HEIGHT];
bool SoftwareRenderer::UseSpriteDeform = false;
Contour SoftwareRenderer::ContourBuffer[MAX_FRAMEBUFFER_HEIGHT];
int SoftwareRenderer::MultTable[0x10000];
int SoftwareRenderer::MultTableInv[0x10000];
int SoftwareRenderer::MultSubTable[0x10000];

BlendState CurrentBlendState;

#if 0
Uint32 ColorAdd(Uint32 color1, Uint32 color2, int percent) {
	Uint32 r = (color1 & 0xFF0000U) + (((color2 & 0xFF0000U) * percent) >> 8);
	Uint32 g = (color1 & 0xFF00U) + (((color2 & 0xFF00U) * percent) >> 8);
	Uint32 b = (color1 & 0xFFU) + (((color2 & 0xFFU) * percent) >> 8);
    if (r > 0xFF0000U) r = 0xFF0000U;
	if (g > 0xFF00U) g = 0xFF00U;
	if (b > 0xFFU) b = 0xFFU;
	return r | g | b;
}
Uint32 ColorSubtract(Uint32 color1, Uint32 color2, int percent) {
    Sint32 r = (color1 & 0xFF0000U) - (((color2 & 0xFF0000U) * percent) >> 8);
	Sint32 g = (color1 & 0xFF00U) - (((color2 & 0xFF00U) * percent) >> 8);
	Sint32 b = (color1 & 0xFFU) - (((color2 & 0xFFU) * percent) >> 8);
    if (r < 0) r = 0;
	if (g < 0) g = 0;
	if (b < 0) b = 0;
	return r | g | b;
}
#endif

#define CLAMP_VAL(v, a, b) \
	if (v < a) \
		v = a; \
	else if (v > b) \
	v = b

Uint8 ColR;
Uint8 ColG;
Uint8 ColB;
Uint32 ColRGB;

PixelFunction CurrentPixelFunction = NULL;
TintFunction CurrentTintFunction = NULL;

bool UseStencil = false;

Uint8 StencilValue = 0x00;
Uint8 StencilMask = 0xFF;

size_t StencilBufferSize = 0;

Uint8 DotMaskH = 0;
Uint8 DotMaskV = 0;

int DotMaskOffsetH = 0;
int DotMaskOffsetV = 0;

#define TRIG_TABLE_BITS 11
#define TRIG_TABLE_SIZE (1 << TRIG_TABLE_BITS)
#define TRIG_TABLE_MASK ((1 << TRIG_TABLE_BITS) - 1)
#define TRIG_TABLE_HALF (TRIG_TABLE_SIZE >> 1)

int SinTable[TRIG_TABLE_SIZE];
int CosTable[TRIG_TABLE_SIZE];

PolygonRenderer polygonRenderer;

int FilterCurrent[0x8000];
int FilterInvert[0x8000];
int FilterBlackAndWhite[0x8000];

// Initialization and disposal functions
void SoftwareRenderer::Init() {
	SoftwareRenderer::BackendFunctions.Init();

	UseStencil = false;
	UseSpriteDeform = false;

	SetDotMask(0);
	SetDotMaskOffsetH(0);
	SetDotMaskOffsetV(0);
}
Uint32 SoftwareRenderer::GetWindowFlags() {
	return Graphics::Internal.GetWindowFlags();
}
void SoftwareRenderer::SetGraphicsFunctions() {
	for (int alpha = 0; alpha < 0x100; alpha++) {
		for (int color = 0; color < 0x100; color++) {
			MultTable[alpha << 8 | color] = (alpha * color) >> 8;
			MultTableInv[alpha << 8 | color] = ((alpha ^ 0xFF) * color) >> 8;
			MultSubTable[alpha << 8 | color] = (alpha * -(color ^ 0xFF)) >> 8;
		}
	}

	for (int a = 0; a < TRIG_TABLE_SIZE; a++) {
		float ang = a * M_PI / TRIG_TABLE_HALF;
		SinTable[a] = (int)(Math::Sin(ang) * TRIG_TABLE_SIZE);
		CosTable[a] = (int)(Math::Cos(ang) * TRIG_TABLE_SIZE);
	}
	for (int a = 0; a < 0x8000; a++) {
		int r = (a >> 10) & 0x1F;
		int g = (a >> 5) & 0x1F;
		int b = (a) & 0x1F;

		int bw = ((r + g + b) / 3) << 3;
		int hex = r << 19 | g << 11 | b << 3;
		FilterBlackAndWhite[a] = bw << 16 | bw << 8 | bw | 0xFF000000U;
		FilterInvert[a] = (hex ^ 0xFFFFFF) | 0xFF000000U;
	}

	CurrentBlendState.Mode = BlendMode_NORMAL;
	CurrentBlendState.Opacity = 0xFF;
	CurrentBlendState.FilterTable = nullptr;

	SoftwareRenderer::BackendFunctions.Init = SoftwareRenderer::Init;
	SoftwareRenderer::BackendFunctions.GetWindowFlags = SoftwareRenderer::GetWindowFlags;
	SoftwareRenderer::BackendFunctions.Dispose = SoftwareRenderer::Dispose;

	// Texture management functions
	SoftwareRenderer::BackendFunctions.CreateTexture = SoftwareRenderer::CreateTexture;
	SoftwareRenderer::BackendFunctions.LockTexture = SoftwareRenderer::LockTexture;
	SoftwareRenderer::BackendFunctions.UpdateTexture = SoftwareRenderer::UpdateTexture;
	SoftwareRenderer::BackendFunctions.UnlockTexture = SoftwareRenderer::UnlockTexture;
	SoftwareRenderer::BackendFunctions.DisposeTexture = SoftwareRenderer::DisposeTexture;

	// Viewport and view-related functions
	SoftwareRenderer::BackendFunctions.SetRenderTarget = SoftwareRenderer::SetRenderTarget;
	SoftwareRenderer::BackendFunctions.ReadFramebuffer = SoftwareRenderer::ReadFramebuffer;
	SoftwareRenderer::BackendFunctions.UpdateWindowSize = SoftwareRenderer::UpdateWindowSize;
	SoftwareRenderer::BackendFunctions.UpdateViewport = SoftwareRenderer::UpdateViewport;
	SoftwareRenderer::BackendFunctions.UpdateClipRect = SoftwareRenderer::UpdateClipRect;
	SoftwareRenderer::BackendFunctions.UpdateOrtho = SoftwareRenderer::UpdateOrtho;
	SoftwareRenderer::BackendFunctions.UpdatePerspective = SoftwareRenderer::UpdatePerspective;
	SoftwareRenderer::BackendFunctions.UpdateProjectionMatrix =
		SoftwareRenderer::UpdateProjectionMatrix;
	SoftwareRenderer::BackendFunctions.MakePerspectiveMatrix =
		SoftwareRenderer::MakePerspectiveMatrix;

	// Shader-related functions
	SoftwareRenderer::BackendFunctions.UseShader = SoftwareRenderer::UseShader;
	SoftwareRenderer::BackendFunctions.SetUniformF = SoftwareRenderer::SetUniformF;
	SoftwareRenderer::BackendFunctions.SetUniformI = SoftwareRenderer::SetUniformI;
	SoftwareRenderer::BackendFunctions.SetUniformTexture = SoftwareRenderer::SetUniformTexture;

	// These guys
	SoftwareRenderer::BackendFunctions.Clear = SoftwareRenderer::Clear;
	SoftwareRenderer::BackendFunctions.Present = SoftwareRenderer::Present;

	// Draw mode setting functions
	SoftwareRenderer::BackendFunctions.SetBlendColor = SoftwareRenderer::SetBlendColor;
	SoftwareRenderer::BackendFunctions.SetBlendMode = SoftwareRenderer::SetBlendMode;
	SoftwareRenderer::BackendFunctions.SetTintColor = SoftwareRenderer::SetTintColor;
	SoftwareRenderer::BackendFunctions.SetTintMode = SoftwareRenderer::SetTintMode;
	SoftwareRenderer::BackendFunctions.SetTintEnabled = SoftwareRenderer::SetTintEnabled;
	SoftwareRenderer::BackendFunctions.SetLineWidth = SoftwareRenderer::SetLineWidth;

	// Primitive drawing functions
	SoftwareRenderer::BackendFunctions.StrokeLine = SoftwareRenderer::StrokeLine;
	SoftwareRenderer::BackendFunctions.StrokeCircle = SoftwareRenderer::StrokeCircle;
	SoftwareRenderer::BackendFunctions.StrokeEllipse = SoftwareRenderer::StrokeEllipse;
	SoftwareRenderer::BackendFunctions.StrokeRectangle = SoftwareRenderer::StrokeRectangle;
	SoftwareRenderer::BackendFunctions.FillCircle = SoftwareRenderer::FillCircle;
	SoftwareRenderer::BackendFunctions.FillEllipse = SoftwareRenderer::FillEllipse;
	SoftwareRenderer::BackendFunctions.FillTriangle = SoftwareRenderer::FillTriangle;
	SoftwareRenderer::BackendFunctions.FillRectangle = SoftwareRenderer::FillRectangle;

	// Texture drawing functions
	SoftwareRenderer::BackendFunctions.DrawTexture = SoftwareRenderer::DrawTexture;
	SoftwareRenderer::BackendFunctions.DrawSprite = SoftwareRenderer::DrawSprite;
	SoftwareRenderer::BackendFunctions.DrawSpritePart = SoftwareRenderer::DrawSpritePart;

	// 3D drawing functions
	SoftwareRenderer::BackendFunctions.DrawPolygon3D = SoftwareRenderer::DrawPolygon3D;
	SoftwareRenderer::BackendFunctions.DrawSceneLayer3D = SoftwareRenderer::DrawSceneLayer3D;
	SoftwareRenderer::BackendFunctions.DrawModel = SoftwareRenderer::DrawModel;
	SoftwareRenderer::BackendFunctions.DrawModelSkinned = SoftwareRenderer::DrawModelSkinned;
	SoftwareRenderer::BackendFunctions.DrawVertexBuffer = SoftwareRenderer::DrawVertexBuffer;
	SoftwareRenderer::BackendFunctions.BindVertexBuffer = SoftwareRenderer::BindVertexBuffer;
	SoftwareRenderer::BackendFunctions.UnbindVertexBuffer =
		SoftwareRenderer::UnbindVertexBuffer;
	SoftwareRenderer::BackendFunctions.BindScene3D = SoftwareRenderer::BindScene3D;
	SoftwareRenderer::BackendFunctions.DrawScene3D = SoftwareRenderer::DrawScene3D;

	SoftwareRenderer::BackendFunctions.SetStencilEnabled = SoftwareRenderer::SetStencilEnabled;
	SoftwareRenderer::BackendFunctions.IsStencilEnabled = SoftwareRenderer::IsStencilEnabled;
	SoftwareRenderer::BackendFunctions.SetStencilTestFunc =
		SoftwareRenderer::SetStencilTestFunc;
	SoftwareRenderer::BackendFunctions.SetStencilPassFunc =
		SoftwareRenderer::SetStencilPassFunc;
	SoftwareRenderer::BackendFunctions.SetStencilFailFunc =
		SoftwareRenderer::SetStencilFailFunc;
	SoftwareRenderer::BackendFunctions.SetStencilValue = SoftwareRenderer::SetStencilValue;
	SoftwareRenderer::BackendFunctions.SetStencilMask = SoftwareRenderer::SetStencilMask;
	SoftwareRenderer::BackendFunctions.ClearStencil = SoftwareRenderer::ClearStencil;

	SoftwareRenderer::BackendFunctions.MakeFrameBufferID = SoftwareRenderer::MakeFrameBufferID;
}
void SoftwareRenderer::Dispose() {}

void SoftwareRenderer::RenderStart() {
	for (int i = 0; i < MAX_PALETTE_COUNT; i++) {
		Graphics::PaletteColors[i][0] &= 0xFFFFFF;
	}
}
void SoftwareRenderer::RenderEnd() {}

// Texture management functions
Texture*
SoftwareRenderer::CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
	Texture* texture = NULL; // Texture::New(format, access, width, height);

	return texture;
}
int SoftwareRenderer::LockTexture(Texture* texture, void** pixels, int* pitch) {
	return 0;
}
int SoftwareRenderer::UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch) {
	return 0;
}
void SoftwareRenderer::UnlockTexture(Texture* texture) {}
void SoftwareRenderer::DisposeTexture(Texture* texture) {}

// Viewport and view-related functions
void SoftwareRenderer::SetRenderTarget(Texture* texture) {}
void SoftwareRenderer::ReadFramebuffer(void* pixels, int width, int height) {
	if (Graphics::Internal.ReadFramebuffer) {
		Graphics::Internal.ReadFramebuffer(pixels, width, height);
	}
}
void SoftwareRenderer::UpdateWindowSize(int width, int height) {
	Graphics::Internal.UpdateWindowSize(width, height);
}
void SoftwareRenderer::UpdateViewport() {
	Graphics::Internal.UpdateViewport();
}
void SoftwareRenderer::UpdateClipRect() {
	Graphics::Internal.UpdateClipRect();
}
void SoftwareRenderer::UpdateOrtho(float left, float top, float right, float bottom) {
	Graphics::Internal.UpdateOrtho(left, top, right, bottom);
}
void SoftwareRenderer::UpdatePerspective(float fovy, float aspect, float nearv, float farv) {
	Graphics::Internal.UpdatePerspective(fovy, aspect, nearv, farv);
}
void SoftwareRenderer::UpdateProjectionMatrix() {
	Graphics::Internal.UpdateProjectionMatrix();
}
void SoftwareRenderer::MakePerspectiveMatrix(Matrix4x4* out,
	float fov,
	float near,
	float far,
	float aspect) {
	float f = 1.0f / tanf(fov / 2.0f);
	float diff = near - far;

	out->Values[0] = f / aspect;
	out->Values[1] = 0.0f;
	out->Values[2] = 0.0f;
	out->Values[3] = 0.0f;

	out->Values[4] = 0.0f;
	out->Values[5] = -f;
	out->Values[6] = 0.0f;
	out->Values[7] = 0.0f;

	out->Values[8] = 0.0f;
	out->Values[9] = 0.0f;
	out->Values[10] = -(far + near) / diff;
	out->Values[11] = 1.0f;

	out->Values[12] = 0.0f;
	out->Values[13] = 0.0f;
	out->Values[14] = -(2.0f * far * near) / diff;
	out->Values[15] = 0.0f;
}

void GetClipRegion(int& clip_x1, int& clip_y1, int& clip_x2, int& clip_y2) {
	if (Graphics::CurrentClip.Enabled) {
		clip_x1 = Graphics::CurrentClip.X;
		clip_y1 = Graphics::CurrentClip.Y;
		clip_x2 = Graphics::CurrentClip.X + Graphics::CurrentClip.Width;
		clip_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;

		if (clip_x1 < 0) {
			clip_x1 = 0;
		}
		if (clip_y1 < 0) {
			clip_y1 = 0;
		}
		if (clip_x2 > (int)Graphics::CurrentRenderTarget->Width) {
			clip_x2 = (int)Graphics::CurrentRenderTarget->Width;
		}
		if (clip_y2 > (int)Graphics::CurrentRenderTarget->Height) {
			clip_y2 = (int)Graphics::CurrentRenderTarget->Height;
		}
	}
	else {
		clip_x1 = 0;
		clip_y1 = 0;
		clip_x2 = (int)Graphics::CurrentRenderTarget->Width;
		clip_y2 = (int)Graphics::CurrentRenderTarget->Height;
	}
}
bool CheckClipRegion(int clip_x1, int clip_y1, int clip_x2, int clip_y2) {
	if (clip_x2 < 0 || clip_y2 < 0 || clip_x1 >= clip_x2 || clip_y1 >= clip_y2) {
		return false;
	}
	return true;
}

// Shader-related functions
void SoftwareRenderer::UseShader(void* shader) {
	if (!shader) {
		CurrentBlendState.FilterTable = nullptr;
		return;
	}

	ObjArray* array = (ObjArray*)shader;

	if (Graphics::PreferredPixelFormat == SDL_PIXELFORMAT_ARGB8888) {
		for (Uint32 i = 0, iSz = (Uint32)array->Values->size(); i < 0x8000 && i < iSz;
			i++) {
			FilterCurrent[i] = AS_INTEGER((*array->Values)[i]) | 0xFF000000U;
		}
	}
	else {
		Uint8 px[4];
		Uint32 newI;
		for (Uint32 i = 0, iSz = (Uint32)array->Values->size(); i < 0x8000 && i < iSz;
			i++) {
			*(Uint32*)&px[0] = AS_INTEGER((*array->Values)[i]);
			newI = (i & 0x1F) << 10 | (i & 0x3E0) | (i & 0x7C00) >> 10;
			FilterCurrent[newI] = px[0] << 16 | px[1] << 8 | px[2] | 0xFF000000U;
		}
	}
	CurrentBlendState.FilterTable = &FilterCurrent[0];
}
void SoftwareRenderer::SetUniformF(int location, int count, float* values) {}
void SoftwareRenderer::SetUniformI(int location, int count, int* values) {}
void SoftwareRenderer::SetUniformTexture(Texture* texture, int uniform_index, int slot) {}

void SoftwareRenderer::SetFilter(int filter) {
	switch (filter) {
	case Filter_NONE:
		CurrentBlendState.FilterTable = nullptr;
		break;
	case Filter_BLACK_AND_WHITE:
		CurrentBlendState.FilterTable = &FilterBlackAndWhite[0];
		break;
	case Filter_INVERT:
		CurrentBlendState.FilterTable = &FilterInvert[0];
		break;
	}
}

// These guys
void SoftwareRenderer::Clear() {
	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;
	memset(dstPx, 0, dstStride * Graphics::CurrentRenderTarget->Height * 4);
}
void SoftwareRenderer::Present() {}

// Draw mode setting functions
#define GET_R(color) ((color >> 16) & 0xFF)
#define GET_G(color) ((color >> 8) & 0xFF)
#define GET_B(color) ((color) & 0xFF)
void SoftwareRenderer::SetColor(Uint32 color) {
	ColRGB = color;
	ColR = GET_R(color);
	ColG = GET_G(color);
	ColB = GET_B(color);
	Graphics::ConvertFromARGBtoNative(&ColRGB, 1);
}
void SoftwareRenderer::SetBlendColor(float r, float g, float b, float a) {
	ColR = (Uint8)(r * 0xFF);
	ColG = (Uint8)(g * 0xFF);
	ColB = (Uint8)(b * 0xFF);
	ColRGB = ColorUtils::ToRGB(ColR, ColG, ColB);
	Graphics::ConvertFromARGBtoNative(&ColRGB, 1);

	int opacity = (int)(a * 0xFF);
	CLAMP_VAL(opacity, 0x00, 0xFF);
	CurrentBlendState.Opacity = opacity;
}
void SoftwareRenderer::SetBlendMode(int srcC, int dstC, int srcA, int dstA) {
	CurrentBlendState.Mode = Graphics::BlendMode;
}
void SoftwareRenderer::SetTintColor(float r, float g, float b, float a) {
	int red = (int)(r * 0xFF);
	int green = (int)(g * 0xFF);
	int blue = (int)(b * 0xFF);
	int alpha = (int)(a * 0x100);

	CLAMP_VAL(red, 0x00, 0xFF);
	CLAMP_VAL(green, 0x00, 0xFF);
	CLAMP_VAL(blue, 0x00, 0xFF);
	CLAMP_VAL(alpha, 0x00, 0x100);

	CurrentBlendState.Tint.Color = red << 16 | green << 8 | blue;
	CurrentBlendState.Tint.Amount = alpha;

	Graphics::ConvertFromARGBtoNative(&CurrentBlendState.Tint.Color, 1);
}
void SoftwareRenderer::SetTintMode(int mode) {
	CurrentBlendState.Tint.Mode = mode;
}
void SoftwareRenderer::SetTintEnabled(bool enabled) {
	CurrentBlendState.Tint.Enabled = enabled;
}

void SoftwareRenderer::Resize(int width, int height) {}

void SoftwareRenderer::SetClip(float x, float y, float width, float height) {}
void SoftwareRenderer::ClearClip() {}

void SoftwareRenderer::Save() {}
void SoftwareRenderer::Translate(float x, float y, float z) {}
void SoftwareRenderer::Rotate(float x, float y, float z) {}
void SoftwareRenderer::Scale(float x, float y, float z) {}
void SoftwareRenderer::Restore() {}

Uint32 SoftwareRenderer::GetBlendColor() {
	return ColorUtils::ToRGB(ColR, ColG, ColB);
}

int SoftwareRenderer::ConvertBlendMode(int blendMode) {
	switch (blendMode) {
	case BlendMode_NORMAL:
		return BlendFlag_TRANSPARENT;
	case BlendMode_ADD:
		return BlendFlag_ADDITIVE;
	case BlendMode_SUBTRACT:
		return BlendFlag_SUBTRACT;
	case BlendMode_MATCH_EQUAL:
		return BlendFlag_MATCH_EQUAL;
	case BlendMode_MATCH_NOT_EQUAL:
		return BlendFlag_MATCH_NOT_EQUAL;
	}
	return BlendFlag_OPAQUE;
}
BlendState SoftwareRenderer::GetBlendState() {
	return CurrentBlendState;
}
bool SoftwareRenderer::AlterBlendState(BlendState& state) {
	int blendMode = ConvertBlendMode(state.Mode);
	int opacity = state.Opacity;

	// Not visible
	if (opacity == 0 && blendMode == BlendFlag_TRANSPARENT) {
		return false;
	}

	// Switch to proper blend flag depending on opacity
	if (opacity != 0 && opacity < 0xFF && blendMode == BlendFlag_OPAQUE) {
		blendMode = BlendFlag_TRANSPARENT;
	}
	else if (opacity == 0xFF && blendMode == BlendFlag_TRANSPARENT) {
		blendMode = BlendFlag_OPAQUE;
	}

	state.Mode = blendMode;

	// Apply tint/filter flags
	if (state.Tint.Enabled) {
		state.Mode |= BlendFlag_TINT_BIT;
	}
	if (state.FilterTable != nullptr) {
		state.Mode |= BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT;
	}

	return true;
}

// Filterless versions
#define ISOLATE_R(color) (color & 0xFF0000)
#define ISOLATE_G(color) (color & 0x00FF00)
#define ISOLATE_B(color) (color & 0x0000FF)

void SoftwareRenderer::PixelNoFiltSetOpaque(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	*dst = *src;
}
void SoftwareRenderer::PixelNoFiltSetTransparent(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	int* multInvTableAt = &MultTableInv[state.Opacity << 8];
	*dst = 0xFF000000U | (multTableAt[GET_R(*src)] + multInvTableAt[GET_R(*dst)]) << 16 |
		(multTableAt[GET_G(*src)] + multInvTableAt[GET_G(*dst)]) << 8 |
		(multTableAt[GET_B(*src)] + multInvTableAt[GET_B(*dst)]);
}
void SoftwareRenderer::PixelNoFiltSetAdditive(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	Uint32 R = (multTableAt[GET_R(*src)] << 16) + ISOLATE_R(*dst);
	Uint32 G = (multTableAt[GET_G(*src)] << 8) + ISOLATE_G(*dst);
	Uint32 B = (multTableAt[GET_B(*src)]) + ISOLATE_B(*dst);
	if (R > 0xFF0000) {
		R = 0xFF0000;
	}
	if (G > 0x00FF00) {
		G = 0x00FF00;
	}
	if (B > 0x0000FF) {
		B = 0x0000FF;
	}
	*dst = 0xFF000000U | R | G | B;
}
void SoftwareRenderer::PixelNoFiltSetSubtract(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	Sint32 R = (multSubTableAt[GET_R(*src)] << 16) + ISOLATE_R(*dst);
	Sint32 G = (multSubTableAt[GET_G(*src)] << 8) + ISOLATE_G(*dst);
	Sint32 B = (multSubTableAt[GET_B(*src)]) + ISOLATE_B(*dst);
	if (R < 0) {
		R = 0;
	}
	if (G < 0) {
		G = 0;
	}
	if (B < 0) {
		B = 0;
	}
	*dst = 0xFF000000U | R | G | B;
}
void SoftwareRenderer::PixelNoFiltSetMatchEqual(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	if ((*dst & 0xFCFCFC) == (SoftwareRenderer::CompareColor & 0xFCFCFC)) {
		*dst = *src;
	}
}
void SoftwareRenderer::PixelNoFiltSetMatchNotEqual(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	if ((*dst & 0xFCFCFC) != (SoftwareRenderer::CompareColor & 0xFCFCFC)) {
		*dst = *src;
	}
}

static PixelFunction PixelNoFiltFunctions[] = {SoftwareRenderer::PixelNoFiltSetOpaque,
	SoftwareRenderer::PixelNoFiltSetTransparent,
	SoftwareRenderer::PixelNoFiltSetAdditive,
	SoftwareRenderer::PixelNoFiltSetSubtract,
	SoftwareRenderer::PixelNoFiltSetMatchEqual,
	SoftwareRenderer::PixelNoFiltSetMatchNotEqual};

// Tinted versions
void SoftwareRenderer::PixelTintSetOpaque(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	*dst = 0xFF000000 | CurrentTintFunction(src, dst, state.Tint.Color, state.Tint.Amount);
}
void SoftwareRenderer::PixelTintSetTransparent(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	Uint32 col =
		0xFF000000 | CurrentTintFunction(src, dst, state.Tint.Color, state.Tint.Amount);
	PixelNoFiltSetTransparent(&col, dst, state, multTableAt, multSubTableAt);
}
void SoftwareRenderer::PixelTintSetAdditive(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	Uint32 col =
		0xFF000000 | CurrentTintFunction(src, dst, state.Tint.Color, state.Tint.Amount);
	PixelNoFiltSetAdditive(&col, dst, state, multTableAt, multSubTableAt);
}
void SoftwareRenderer::PixelTintSetSubtract(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	Uint32 col =
		0xFF000000 | CurrentTintFunction(src, dst, state.Tint.Color, state.Tint.Amount);
	PixelNoFiltSetSubtract(&col, dst, state, multTableAt, multSubTableAt);
}
void SoftwareRenderer::PixelTintSetMatchEqual(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	if ((*dst & 0xFCFCFC) == (SoftwareRenderer::CompareColor & 0xFCFCFC)) {
		*dst = 0xFF000000 |
			CurrentTintFunction(src, dst, state.Tint.Color, state.Tint.Amount);
	}
}
void SoftwareRenderer::PixelTintSetMatchNotEqual(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	if ((*dst & 0xFCFCFC) != (SoftwareRenderer::CompareColor & 0xFCFCFC)) {
		*dst = 0xFF000000 |
			CurrentTintFunction(src, dst, state.Tint.Color, state.Tint.Amount);
	}
}

static PixelFunction PixelTintFunctions[] = {SoftwareRenderer::PixelTintSetOpaque,
	SoftwareRenderer::PixelTintSetTransparent,
	SoftwareRenderer::PixelTintSetAdditive,
	SoftwareRenderer::PixelTintSetSubtract,
	SoftwareRenderer::PixelTintSetMatchEqual,
	SoftwareRenderer::PixelTintSetMatchNotEqual};

#define GET_FILTER_COLOR(col) ((col & 0xF80000) >> 9 | (col & 0xF800) >> 6 | (col & 0xF8) >> 3)

static Uint32 TintNormalSource(Uint32* src, Uint32* dst, Uint32 tintColor, Uint32 tintAmount) {
	return ColorUtils::Tint(*src, tintColor, tintAmount);
}
static Uint32 TintNormalDest(Uint32* src, Uint32* dst, Uint32 tintColor, Uint32 tintAmount) {
	return ColorUtils::Tint(*dst, tintColor, tintAmount);
}
static Uint32 TintBlendSource(Uint32* src, Uint32* dst, Uint32 tintColor, Uint32 tintAmount) {
	return ColorUtils::Blend(*src, tintColor, tintAmount);
}
static Uint32 TintBlendDest(Uint32* src, Uint32* dst, Uint32 tintColor, Uint32 tintAmount) {
	return ColorUtils::Blend(*dst, tintColor, tintAmount);
}
static Uint32 TintFilterSource(Uint32* src, Uint32* dst, Uint32 tintColor, Uint32 tintAmount) {
	return CurrentBlendState.FilterTable[GET_FILTER_COLOR(*src)];
}
static Uint32 TintFilterDest(Uint32* src, Uint32* dst, Uint32 tintColor, Uint32 tintAmount) {
	return CurrentBlendState.FilterTable[GET_FILTER_COLOR(*dst)];
}

void SoftwareRenderer::SetTintFunction(int blendFlags) {
	TintFunction tintFunctions[] = {
		TintNormalSource, TintNormalDest, TintBlendSource, TintBlendDest};

	TintFunction filterFunctions[] = {TintFilterSource, TintFilterDest};

	if (blendFlags & BlendFlag_FILTER_BIT) {
		CurrentTintFunction = filterFunctions[CurrentBlendState.Tint.Mode & 1];
	}
	else if (blendFlags & BlendFlag_TINT_BIT) {
		CurrentTintFunction = tintFunctions[CurrentBlendState.Tint.Mode];
	}
}

// Stencil ops (test)
static bool StencilTestNever(Uint8* buf, Uint8 value, Uint8 mask) {
	return false;
}
static bool StencilTestAlways(Uint8* buf, Uint8 value, Uint8 mask) {
	return true;
}
static bool StencilTestEqual(Uint8* buf, Uint8 value, Uint8 mask) {
	return (value & mask) == (*buf & mask);
}
static bool StencilTestNotEqual(Uint8* buf, Uint8 value, Uint8 mask) {
	return (value & mask) != (*buf & mask);
}
static bool StencilTestLess(Uint8* buf, Uint8 value, Uint8 mask) {
	return (value & mask) < (*buf & mask);
}
static bool StencilTestGreater(Uint8* buf, Uint8 value, Uint8 mask) {
	return (value & mask) > (*buf & mask);
}
static bool StencilTestLEqual(Uint8* buf, Uint8 value, Uint8 mask) {
	return (value & mask) <= (*buf & mask);
}
static bool StencilTestGEqual(Uint8* buf, Uint8 value, Uint8 mask) {
	return (value & mask) >= (*buf & mask);
}

// Stencil ops (write)
static void StencilOpKeep(Uint8* buf, Uint8 value) {
	// Do nothing
}
static void StencilOpZero(Uint8* buf, Uint8 value) {
	*buf = 0;
}
static void StencilOpIncr(Uint8* buf, Uint8 value) {
	int val = *buf + 1;
	if (val > 255) {
		val = 255;
	}
	*buf = (Uint8)val;
}
static void StencilOpDecr(Uint8* buf, Uint8 value) {
	int val = *buf - 1;
	if (val < 0) {
		val = 0;
	}
	*buf = (Uint8)val;
}
static void StencilOpInvert(Uint8* buf, Uint8 value) {
	*buf = ~(*buf);
}
static void StencilOpReplace(Uint8* buf, Uint8 value) {
	*buf = value;
}
static void StencilOpIncrWrap(Uint8* buf, Uint8 value) {
	value = *buf;
	value++;
	*buf = value;
}
static void StencilOpDecrWrap(Uint8* buf, Uint8 value) {
	value = *buf;
	value--;
	*buf = value;
}

// Stencil buffer funcs
static StencilOpFunction StencilOpFunctionList[] = {StencilOpKeep,
	StencilOpZero,
	StencilOpIncr,
	StencilOpDecr,
	StencilOpInvert,
	StencilOpReplace,
	StencilOpIncrWrap,
	StencilOpDecrWrap};

// Stencil buffer management
StencilTestFunction StencilFuncTest = StencilTestAlways;
StencilOpFunction StencilFuncPass = StencilOpKeep;
StencilOpFunction StencilFuncFail = StencilOpKeep;

void SoftwareRenderer::SetStencilEnabled(bool enabled) {
	if (Scene::ViewCurrent >= 0) {
		UseStencil = enabled;
		Scene::Views[Scene::ViewCurrent].SetStencilEnabled(enabled);
	}
}
bool SoftwareRenderer::IsStencilEnabled() {
	return UseStencil;
}
void SoftwareRenderer::SetStencilTestFunc(int stencilTest) {
	StencilTestFunction funcList[] = {StencilTestNever,
		StencilTestAlways,
		StencilTestEqual,
		StencilTestNotEqual,
		StencilTestLess,
		StencilTestGreater,
		StencilTestLEqual,
		StencilTestGEqual};

	if (stencilTest >= StencilTest_Never && stencilTest <= StencilTest_GEqual) {
		StencilFuncTest = funcList[stencilTest];
	}
}
void SoftwareRenderer::SetStencilPassFunc(int stencilOp) {
	if (stencilOp >= StencilOp_Keep && stencilOp <= StencilOp_DecrWrap) {
		StencilFuncPass = StencilOpFunctionList[stencilOp];
	}
}
void SoftwareRenderer::SetStencilFailFunc(int stencilOp) {
	if (stencilOp >= StencilOp_Keep && stencilOp <= StencilOp_DecrWrap) {
		StencilFuncFail = StencilOpFunctionList[stencilOp];
	}
}
void SoftwareRenderer::SetStencilValue(int value) {
	StencilValue = value;
}
void SoftwareRenderer::SetStencilMask(int mask) {
	StencilMask = mask;
}
void SoftwareRenderer::ClearStencil() {
	if (UseStencil && Graphics::CurrentView) {
		Graphics::CurrentView->ClearStencil();
	}
}

void SoftwareRenderer::PixelStencil(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	size_t pos = dst - (Uint32*)Graphics::CurrentRenderTarget->Pixels;

	View* currentView = Graphics::CurrentView;
	Uint8* buffer = &currentView->StencilBuffer[pos];
	if (StencilFuncTest(buffer, StencilValue, StencilMask)) {
		CurrentPixelFunction(src, dst, state, multTableAt, multSubTableAt);
		StencilFuncPass(buffer, StencilValue);
	}
	else {
		StencilFuncFail(buffer, StencilValue);
	}
}

void SoftwareRenderer::SetDotMask(int mask) {
	SetDotMaskH(mask);
	SetDotMaskV(mask);
}
void SoftwareRenderer::SetDotMaskH(int mask) {
	if (mask < 0) {
		mask = 0;
	}
	else if (mask > 255) {
		mask = 255;
	}

	DotMaskH = mask;
}
void SoftwareRenderer::SetDotMaskV(int mask) {
	if (mask < 0) {
		mask = 0;
	}
	else if (mask > 255) {
		mask = 255;
	}

	DotMaskV = mask;
}
void SoftwareRenderer::SetDotMaskOffsetH(int offset) {
	DotMaskOffsetH = offset;
}
void SoftwareRenderer::SetDotMaskOffsetV(int offset) {
	DotMaskOffsetV = offset;
}

void SoftwareRenderer::PixelDotMaskH(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	size_t pos = dst - (Uint32*)Graphics::CurrentRenderTarget->Pixels;

	int x = (pos % Graphics::CurrentRenderTarget->Width) + DotMaskOffsetH;
	if (x & DotMaskH) {
		return;
	}

	if (UseStencil) {
		PixelStencil(src, dst, state, multTableAt, multSubTableAt);
	}
	else {
		CurrentPixelFunction(src, dst, state, multTableAt, multSubTableAt);
	}
}
void SoftwareRenderer::PixelDotMaskV(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	size_t pos = dst - (Uint32*)Graphics::CurrentRenderTarget->Pixels;

	int y = (pos / Graphics::CurrentRenderTarget->Width) + DotMaskOffsetV;
	if (y & DotMaskV) {
		return;
	}

	if (UseStencil) {
		PixelStencil(src, dst, state, multTableAt, multSubTableAt);
	}
	else {
		CurrentPixelFunction(src, dst, state, multTableAt, multSubTableAt);
	}
}
void SoftwareRenderer::PixelDotMaskHV(Uint32* src,
	Uint32* dst,
	BlendState& state,
	int* multTableAt,
	int* multSubTableAt) {
	size_t pos = dst - (Uint32*)Graphics::CurrentRenderTarget->Pixels;

	int x = (pos % Graphics::CurrentRenderTarget->Width) + DotMaskOffsetH;
	int y = (pos / Graphics::CurrentRenderTarget->Width) + DotMaskOffsetV;
	if (x & DotMaskH || y & DotMaskV) {
		return;
	}

	if (UseStencil) {
		PixelStencil(src, dst, state, multTableAt, multSubTableAt);
	}
	else {
		CurrentPixelFunction(src, dst, state, multTableAt, multSubTableAt);
	}
}

// TODO: Material support
static int CalcVertexColor(Scene3D* scene, VertexAttribute* vertex, int normalY) {
	int col_r = GET_R(vertex->Color);
	int col_g = GET_G(vertex->Color);
	int col_b = GET_B(vertex->Color);
	int specularR = 0, specularG = 0, specularB = 0;

	Uint32 lightingAmbientR = (Uint32)(scene->Lighting.Ambient.R * 0x100);
	Uint32 lightingAmbientG = (Uint32)(scene->Lighting.Ambient.G * 0x100);
	Uint32 lightingAmbientB = (Uint32)(scene->Lighting.Ambient.B * 0x100);

	Uint32 lightingDiffuseR = Math::CeilPOT((int)(scene->Lighting.Diffuse.R * 0x100));
	Uint32 lightingDiffuseG = Math::CeilPOT((int)(scene->Lighting.Diffuse.G * 0x100));
	Uint32 lightingDiffuseB = Math::CeilPOT((int)(scene->Lighting.Diffuse.B * 0x100));

	Uint32 lightingSpecularR = Math::CeilPOT((int)(scene->Lighting.Specular.R * 0x100));
	Uint32 lightingSpecularG = Math::CeilPOT((int)(scene->Lighting.Specular.G * 0x100));
	Uint32 lightingSpecularB = Math::CeilPOT((int)(scene->Lighting.Specular.B * 0x100));

#define SHIFT_COL(color) \
	{ \
		int v = 0; \
		while (color) { \
			color >>= 1; \
			v++; \
		} \
		color = --v; \
	}

	SHIFT_COL(lightingDiffuseR);
	SHIFT_COL(lightingDiffuseG);
	SHIFT_COL(lightingDiffuseB);

	SHIFT_COL(lightingSpecularR);
	SHIFT_COL(lightingSpecularG);
	SHIFT_COL(lightingSpecularB);

#undef SHIFT_COL

	int ambientNormalY = normalY >> 10;
	int reweightedNormal = (normalY >> 2) * (abs(normalY) >> 2);

	// r
	col_r = (col_r * (ambientNormalY + lightingAmbientR)) >> lightingDiffuseR;
	specularR = reweightedNormal >> 6 >> lightingSpecularR;
	CLAMP_VAL(specularR, 0x00, 0xFF);
	specularR += col_r;
	CLAMP_VAL(specularR, 0x00, 0xFF);
	col_r = specularR;

	// g
	col_g = (col_g * (ambientNormalY + lightingAmbientG)) >> lightingDiffuseG;
	specularG = reweightedNormal >> 6 >> lightingSpecularG;
	CLAMP_VAL(specularG, 0x00, 0xFF);
	specularG += col_g;
	CLAMP_VAL(specularG, 0x00, 0xFF);
	col_g = specularG;

	// b
	col_b = (col_b * (ambientNormalY + lightingAmbientB)) >> lightingDiffuseB;
	specularB = reweightedNormal >> 6 >> lightingSpecularB;
	CLAMP_VAL(specularB, 0x00, 0xFF);
	specularB += col_b;
	CLAMP_VAL(specularB, 0x00, 0xFF);
	col_b = specularB;

	return col_r << 16 | col_g << 8 | col_b;
}

// Drawing 3D
void SoftwareRenderer::BindVertexBuffer(Uint32 vertexBufferIndex) {}
void SoftwareRenderer::UnbindVertexBuffer() {}
void SoftwareRenderer::BindScene3D(Uint32 sceneIndex) {
	if (sceneIndex < 0 || sceneIndex >= MAX_3D_SCENES) {
		return;
	}

	Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];
	if (scene->ClipPolygons) {
		polygonRenderer.BuildFrustumPlanes(
			scene->NearClippingPlane, scene->FarClippingPlane);
		polygonRenderer.ClipPolygonsByFrustum = true;
	}
	else {
		polygonRenderer.ClipPolygonsByFrustum = false;
	}
}
void SoftwareRenderer::DrawScene3D(Uint32 sceneIndex, Uint32 drawMode) {
	if (sceneIndex < 0 || sceneIndex >= MAX_3D_SCENES) {
		return;
	}

	Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];
	if (!scene->Initialized) {
		return;
	}

	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	Matrix4x4* out = Graphics::ModelViewMatrix;
	int x = out->Values[12];
	int y = out->Values[13];
	x -= cx;
	y -= cy;

	bool doDepthTest = drawMode & DrawMode_DEPTH_TEST;
	bool usePerspective = !(drawMode & DrawMode_ORTHOGRAPHIC);

	PolygonRasterizer::SetDepthTest(doDepthTest);

	BlendState blendState;

#define SET_BLENDFLAG_AND_OPACITY(face) \
	blendState = {0}; \
	if (!Graphics::TextureBlend) { \
		blendState.Mode = BlendFlag_OPAQUE; \
		blendState.Opacity = 0xFF; \
	} \
	else { \
		blendState = face->Blend; \
		AlterBlendState(blendState); \
		if (!Graphics::UseTinting) \
			blendState.Tint.Enabled = false; \
	} \
	if ((blendState.Mode & BlendFlag_MODE_MASK) != BlendFlag_OPAQUE) \
		useDepthBuffer = false; \
	else \
		useDepthBuffer = doDepthTest; \
	PolygonRasterizer::SetUseDepthBuffer(useDepthBuffer)

	VertexBuffer* vertexBuffer = scene->Buffer;
	VertexAttribute* vertexAttribsPtr = vertexBuffer->Vertices; // R
	FaceInfo* faceInfoPtr = vertexBuffer->FaceInfoBuffer; // RW

	bool sortFaces = !doDepthTest && vertexBuffer->FaceCount > 1;
	if (Graphics::TextureBlend) {
		sortFaces = true;
	}

	// Convert vertex colors to native format
	if (Graphics::PreferredPixelFormat != SDL_PIXELFORMAT_ARGB8888) {
		VertexAttribute* vertex = vertexAttribsPtr;
		for (Uint32 i = 0; i < vertexBuffer->VertexCount; i++, vertex++) {
			Graphics::ConvertFromARGBtoNative(&vertex->Color, 1);
		}
	}

	// Get the face depth and vertices' start index
	Uint32 verticesStartIndex = 0;
	for (Uint32 f = 0; f < vertexBuffer->FaceCount; f++) {
		Uint32 vertexCount = faceInfoPtr->NumVertices;

		// Average the Z coordinates of the face
		if (sortFaces) {
			Sint64 depth = vertexAttribsPtr[0].Position.Z;
			for (Uint32 i = 1; i < vertexCount; i++) {
				depth += vertexAttribsPtr[i].Position.Z;
			}

			faceInfoPtr->Depth = depth / vertexCount;
			vertexAttribsPtr += vertexCount;
		}

		faceInfoPtr->DrawMode |= drawMode;
		faceInfoPtr->VerticesStartIndex = verticesStartIndex;
		verticesStartIndex += vertexCount;

		faceInfoPtr++;
	}

	// Sort face infos by depth
	if (sortFaces) {
		qsort(vertexBuffer->FaceInfoBuffer,
			vertexBuffer->FaceCount,
			sizeof(FaceInfo),
			PolygonRenderer::FaceSortFunction);
	}

	// sas
	for (Uint32 f = 0; f < vertexBuffer->FaceCount; f++) {
		Vector3 polygonVertex[MAX_POLYGON_VERTICES];
		Vector2 polygonUV[MAX_POLYGON_VERTICES];
		Uint32 polygonVertexIndex = 0;
		Uint32 numOutside = 0;

		faceInfoPtr = &vertexBuffer->FaceInfoBuffer[f];

		bool doAffineMapping = faceInfoPtr->DrawMode & DrawMode_AFFINE;
		bool useDepthBuffer;

		VertexAttribute *vertex, *vertexFirst;
		Uint32 vertexCount, vertexCountPerFace, vertexCountPerFaceMinus1;
		Texture* texturePtr;

		int widthSubpx = (int)(currentView->Width) << 16;
		int heightSubpx = (int)(currentView->Height) << 16;
		int widthHalfSubpx = widthSubpx >> 1;
		int heightHalfSubpx = heightSubpx >> 1;

#define PROJECT_X_WITH_Z(pointX, pointZ) \
	((pointX * currentView->Width * 0x10000) / pointZ) - (cx << 16) + widthHalfSubpx
#define PROJECT_Y_WITH_Z(pointY, pointZ) \
	((pointY * currentView->Height * 0x10000) / pointZ) - (cy << 16) + heightHalfSubpx

#define PROJECT_X(pointX) PROJECT_X_WITH_Z(pointX, vertexZ)
#define PROJECT_Y(pointY) PROJECT_Y_WITH_Z(pointY, vertexZ)

#define ORTHO_X(pointX) pointX - (cx << 16) + widthHalfSubpx
#define ORTHO_Y(pointY) pointY - (cy << 16) + heightHalfSubpx

		if (faceInfoPtr->DrawMode & DrawMode_FOG) {
			PolygonRasterizer::SetUseFog(true);
			PolygonRasterizer::SetFogColor(
				scene->Fog.Color.R, scene->Fog.Color.G, scene->Fog.Color.B);
			PolygonRasterizer::SetFogSmoothness(scene->Fog.Smoothness);
			PolygonRasterizer::SetFogEquation(scene->Fog.Equation);

			switch (scene->Fog.Equation) {
			case FogEquation_Linear:
				PolygonRasterizer::SetFogStart(scene->Fog.Start);
				PolygonRasterizer::SetFogEnd(scene->Fog.End);
				break;
			case FogEquation_Exp:
				PolygonRasterizer::SetFogDensity(scene->Fog.Density);
				break;
			}
		}
		else {
			PolygonRasterizer::SetUseFog(false);
		}

		switch (faceInfoPtr->DrawMode & DrawMode_FillTypeMask) {
		// Lines, Solid Colored
		case DrawMode_LINES:
			vertexCountPerFaceMinus1 = faceInfoPtr->NumVertices - 1;
			vertexFirst = &vertexBuffer->Vertices[faceInfoPtr->VerticesStartIndex];
			vertex = vertexFirst;

			SET_BLENDFLAG_AND_OPACITY(faceInfoPtr);
			CurrentBlendState = blendState;

			if (usePerspective) {
#define LINE_X(pos, z) ((float)PROJECT_X_WITH_Z(pos, z)) / 0x10000
#define LINE_Y(pos, z) ((float)PROJECT_Y_WITH_Z(pos, z)) / 0x10000
				while (vertexCountPerFaceMinus1--) {
					int vertexZ = vertex->Position.Z;
					if (vertexZ < 0x10000) {
						goto mrt_line_solid_NEXT_FACE;
					}

					SetColor(vertex->Color);
					SoftwareRenderer::StrokeLine(
						LINE_X(vertex[0].Position.X, vertex[0].Position.Z),
						LINE_Y(vertex[0].Position.Y, vertex[0].Position.Z),
						LINE_X(vertex[1].Position.X, vertex[1].Position.Z),
						LINE_Y(vertex[1].Position.Y, vertex[1].Position.Z));
					vertex++;
				}
				int vertexZ = vertex->Position.Z;
				if (vertexZ < 0x10000) {
					goto mrt_line_solid_NEXT_FACE;
				}
				SetColor(vertex->Color);
				SoftwareRenderer::StrokeLine(
					LINE_X(vertex->Position.X, vertex->Position.Z),
					LINE_Y(vertex->Position.Y, vertex->Position.Z),
					LINE_X(vertexFirst->Position.X, vertexFirst->Position.Z),
					LINE_Y(vertexFirst->Position.Y, vertexFirst->Position.Z));
			}
			else {
#define LINE_ORTHO_X(pos) ((float)ORTHO_X(pos)) / 0x10000
#define LINE_ORTHO_Y(pos) ((float)ORTHO_Y(pos)) / 0x10000
				while (vertexCountPerFaceMinus1--) {
					SetColor(vertex->Color);
					SoftwareRenderer::StrokeLine(
						LINE_ORTHO_X(vertex[0].Position.X),
						LINE_ORTHO_Y(vertex[0].Position.Y),
						LINE_ORTHO_X(vertex[1].Position.X),
						LINE_ORTHO_Y(vertex[1].Position.Y));
					vertex++;
				}
				SetColor(vertex->Color);
				SoftwareRenderer::StrokeLine(LINE_ORTHO_X(vertex->Position.X),
					LINE_ORTHO_Y(vertex->Position.Y),
					LINE_ORTHO_X(vertexFirst->Position.X),
					LINE_ORTHO_Y(vertexFirst->Position.Y));
			}

		mrt_line_solid_NEXT_FACE:
			faceInfoPtr++;
			break;
		// Lines, Flat Shading
		case DrawMode_LINES | DrawMode_FLAT_LIGHTING:
		// Lines, Smooth Shading
		case DrawMode_LINES | DrawMode_SMOOTH_LIGHTING: {
			vertexCount = faceInfoPtr->NumVertices;
			vertexCountPerFaceMinus1 = vertexCount - 1;
			vertexFirst = &vertexBuffer->Vertices[faceInfoPtr->VerticesStartIndex];
			vertex = vertexFirst;

			int averageNormalY = vertex[0].Normal.Y;
			for (Uint32 i = 1; i < vertexCount; i++) {
				averageNormalY += vertex[i].Normal.Y;
			}
			averageNormalY /= vertexCount;

			SetColor(CalcVertexColor(scene, vertex, averageNormalY >> 8));
			SET_BLENDFLAG_AND_OPACITY(faceInfoPtr);
			CurrentBlendState = blendState;

			if (usePerspective) {
				while (vertexCountPerFaceMinus1--) {
					int vertexZ = vertex->Position.Z;
					if (vertexZ < 0x10000) {
						goto mrt_line_smooth_NEXT_FACE;
					}

					SoftwareRenderer::StrokeLine(
						LINE_X(vertex[0].Position.X, vertex[0].Position.Z),
						LINE_Y(vertex[0].Position.Y, vertex[0].Position.Z),
						LINE_X(vertex[1].Position.X, vertex[1].Position.Z),
						LINE_Y(vertex[1].Position.Y, vertex[1].Position.Z));
					vertex++;
				}
				int vertexZ = vertex->Position.Z;
				if (vertexZ < 0x10000) {
					goto mrt_line_smooth_NEXT_FACE;
				}
				SoftwareRenderer::StrokeLine(
					LINE_X(vertex->Position.X, vertex->Position.Z),
					LINE_Y(vertex->Position.Y, vertex->Position.Z),
					LINE_X(vertexFirst->Position.X, vertexFirst->Position.Z),
					LINE_Y(vertexFirst->Position.Y, vertexFirst->Position.Z));
#undef LINE_X
#undef LINE_Y
			}
			else {
				while (vertexCountPerFaceMinus1--) {
					SoftwareRenderer::StrokeLine(
						LINE_ORTHO_X(vertex[0].Position.X),
						LINE_ORTHO_Y(vertex[0].Position.Y),
						LINE_ORTHO_X(vertex[1].Position.X),
						LINE_ORTHO_Y(vertex[1].Position.Y));
					vertex++;
				}
				SoftwareRenderer::StrokeLine(LINE_ORTHO_X(vertex->Position.X),
					LINE_ORTHO_Y(vertex->Position.Y),
					LINE_ORTHO_X(vertexFirst->Position.X),
					LINE_ORTHO_Y(vertexFirst->Position.Y));
#undef LINE_ORTHO_X
#undef LINE_ORTHO_Y
			}

		mrt_line_smooth_NEXT_FACE:
			break;
		}
		// Polygons, Solid Colored
		case DrawMode_POLYGONS:
			vertexCount = vertexCountPerFace = faceInfoPtr->NumVertices;
			vertexFirst = &vertexBuffer->Vertices[faceInfoPtr->VerticesStartIndex];
			vertex = vertexFirst;

			SET_BLENDFLAG_AND_OPACITY(faceInfoPtr);

			while (vertexCountPerFace--) {
				int vertexZ = vertex->Position.Z;
				if (usePerspective) {
					if (vertexZ < 0x10000) {
						goto mrt_poly_solid_NEXT_FACE;
					}

					polygonVertex[polygonVertexIndex].X =
						PROJECT_X(vertex->Position.X);
					polygonVertex[polygonVertexIndex].Y =
						PROJECT_Y(vertex->Position.Y);
				}
				else {
					polygonVertex[polygonVertexIndex].X =
						ORTHO_X(vertex->Position.X);
					polygonVertex[polygonVertexIndex].Y =
						ORTHO_Y(vertex->Position.Y);
				}

#define POINT_IS_OUTSIDE(i) \
	(polygonVertex[i].X < 0 || polygonVertex[i].Y < 0 || polygonVertex[i].X > widthSubpx || \
		polygonVertex[i].Y > heightSubpx)

				if (POINT_IS_OUTSIDE(polygonVertexIndex)) {
					numOutside++;
				}

				polygonVertex[polygonVertexIndex].Z = vertexZ;
				polygonUV[polygonVertexIndex].X = vertex->UV.X;
				polygonUV[polygonVertexIndex].Y = vertex->UV.Y;
				polygonVertexIndex++;
				vertex++;
			}

			if (numOutside == vertexCount) {
				break;
			}

			texturePtr = nullptr;
			if (faceInfoPtr->DrawMode & DrawMode_TEXTURED) {
				if (faceInfoPtr->UseMaterial) {
					texturePtr = (Texture*)faceInfoPtr->MaterialInfo.Texture;
				}
			}

			if (texturePtr) {
				if (!doAffineMapping) {
					PolygonRasterizer::DrawPerspective(texturePtr,
						polygonVertex,
						polygonUV,
						vertexFirst->Color,
						vertexCount,
						blendState);
				}
				else {
					PolygonRasterizer::DrawAffine(texturePtr,
						polygonVertex,
						polygonUV,
						vertexFirst->Color,
						vertexCount,
						blendState);
				}
			}
			else {
				if (useDepthBuffer) {
					PolygonRasterizer::DrawDepth(polygonVertex,
						vertexFirst->Color,
						vertexCount,
						blendState);
				}
				else {
					PolygonRasterizer::DrawShaded(polygonVertex,
						vertexFirst->Color,
						vertexCount,
						blendState);
				}
			}

		mrt_poly_solid_NEXT_FACE:
			break;
		// Polygons, Flat Shading
		case DrawMode_POLYGONS | DrawMode_FLAT_LIGHTING: {
			vertexCount = vertexCountPerFace = faceInfoPtr->NumVertices;
			vertexFirst = &vertexBuffer->Vertices[faceInfoPtr->VerticesStartIndex];
			vertex = vertexFirst;

			int averageNormalY = vertex[0].Normal.Y;
			for (Uint32 i = 1; i < vertexCount; i++) {
				averageNormalY += vertex[i].Normal.Y;
			}
			averageNormalY /= vertexCount;

			int color = CalcVertexColor(scene, vertex, averageNormalY >> 8);
			SET_BLENDFLAG_AND_OPACITY(faceInfoPtr);

			while (vertexCountPerFace--) {
				int vertexZ = vertex->Position.Z;
				if (usePerspective) {
					if (vertexZ < 0x10000) {
						goto mrt_poly_flat_NEXT_FACE;
					}

					polygonVertex[polygonVertexIndex].X =
						PROJECT_X(vertex->Position.X);
					polygonVertex[polygonVertexIndex].Y =
						PROJECT_Y(vertex->Position.Y);
				}
				else {
					polygonVertex[polygonVertexIndex].X =
						ORTHO_X(vertex->Position.X);
					polygonVertex[polygonVertexIndex].Y =
						ORTHO_Y(vertex->Position.Y);
				}

				if (POINT_IS_OUTSIDE(polygonVertexIndex)) {
					numOutside++;
				}

				polygonVertex[polygonVertexIndex].Z = vertexZ;
				polygonUV[polygonVertexIndex].X = vertex->UV.X;
				polygonUV[polygonVertexIndex].Y = vertex->UV.Y;
				polygonVertexIndex++;
				vertex++;
			}

			if (numOutside == vertexCount) {
				break;
			}

			texturePtr = nullptr;
			if (faceInfoPtr->DrawMode & DrawMode_TEXTURED) {
				if (faceInfoPtr->UseMaterial) {
					texturePtr = (Texture*)faceInfoPtr->MaterialInfo.Texture;
				}
			}

			if (texturePtr) {
				if (!doAffineMapping) {
					PolygonRasterizer::DrawPerspective(texturePtr,
						polygonVertex,
						polygonUV,
						color,
						vertexCount,
						blendState);
				}
				else {
					PolygonRasterizer::DrawAffine(texturePtr,
						polygonVertex,
						polygonUV,
						color,
						vertexCount,
						blendState);
				}
			}
			else {
				if (useDepthBuffer) {
					PolygonRasterizer::DrawDepth(
						polygonVertex, color, vertexCount, blendState);
				}
				else {
					PolygonRasterizer::DrawShaded(
						polygonVertex, color, vertexCount, blendState);
				}
			}

		mrt_poly_flat_NEXT_FACE:
			break;
		}
		// Polygons, Smooth Shading
		case DrawMode_POLYGONS | DrawMode_SMOOTH_LIGHTING:
			vertexCount = vertexCountPerFace = faceInfoPtr->NumVertices;
			vertexFirst = &vertexBuffer->Vertices[faceInfoPtr->VerticesStartIndex];
			vertex = vertexFirst;

			SET_BLENDFLAG_AND_OPACITY(faceInfoPtr);

			Vector3 polygonVertex[MAX_POLYGON_VERTICES];
			Vector2 polygonUV[MAX_POLYGON_VERTICES];
			int polygonVertColor[MAX_POLYGON_VERTICES];
			Uint32 polygonVertexIndex = 0;
			Uint32 numOutside = 0;
			while (vertexCountPerFace--) {
				int vertexZ = vertex->Position.Z;
				if (usePerspective) {
					if (vertexZ < 0x10000) {
						goto mrt_poly_smooth_NEXT_FACE;
					}

					polygonVertex[polygonVertexIndex].X =
						PROJECT_X(vertex->Position.X);
					polygonVertex[polygonVertexIndex].Y =
						PROJECT_Y(vertex->Position.Y);
				}
				else {
					polygonVertex[polygonVertexIndex].X =
						ORTHO_X(vertex->Position.X);
					polygonVertex[polygonVertexIndex].Y =
						ORTHO_Y(vertex->Position.Y);
				}

				if (POINT_IS_OUTSIDE(polygonVertexIndex)) {
					numOutside++;
				}

				polygonVertex[polygonVertexIndex].Z = vertexZ;

				polygonUV[polygonVertexIndex].X = vertex->UV.X;
				polygonUV[polygonVertexIndex].Y = vertex->UV.Y;

				polygonVertColor[polygonVertexIndex] =
					CalcVertexColor(scene, vertex, vertex->Normal.Y >> 8);
				polygonVertexIndex++;
				vertex++;
			}

			if (numOutside == vertexCount) {
				break;
			}

#undef POINT_IS_OUTSIDE

			texturePtr = nullptr;
			if (faceInfoPtr->DrawMode & DrawMode_TEXTURED) {
				if (faceInfoPtr->UseMaterial) {
					texturePtr = (Texture*)faceInfoPtr->MaterialInfo.Texture;
				}
			}

			if (texturePtr) {
				if (!doAffineMapping) {
					PolygonRasterizer::DrawBlendPerspective(texturePtr,
						polygonVertex,
						polygonUV,
						polygonVertColor,
						vertexCount,
						blendState);
				}
				else {
					PolygonRasterizer::DrawBlendAffine(texturePtr,
						polygonVertex,
						polygonUV,
						polygonVertColor,
						vertexCount,
						blendState);
				}
			}
			else {
				if (useDepthBuffer) {
					PolygonRasterizer::DrawBlendDepth(polygonVertex,
						polygonVertColor,
						vertexCount,
						blendState);
				}
				else {
					PolygonRasterizer::DrawBlendShaded(polygonVertex,
						polygonVertColor,
						vertexCount,
						blendState);
				}
			}

		mrt_poly_smooth_NEXT_FACE:
			break;
		}
	}

#undef SET_BLENDFLAG_AND_OPACITY

#undef PROJECT_X
#undef PROJECT_Y

#undef ORTHO_X
#undef ORTHO_Y
}

bool SoftwareRenderer::SetupPolygonRenderer(Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {
	if (!polygonRenderer.SetBuffers()) {
		return false;
	}

	polygonRenderer.DrawMode =
		polygonRenderer.ScenePtr != nullptr ? polygonRenderer.ScenePtr->DrawMode : 0;
	polygonRenderer.DoProjection = polygonRenderer.ScenePtr != nullptr;
	polygonRenderer.DoClipping = polygonRenderer.ScenePtr != nullptr;
	polygonRenderer.ModelMatrix = modelMatrix;
	polygonRenderer.NormalMatrix = normalMatrix;
	polygonRenderer.CurrentColor = GetBlendColor();

	return true;
}

void SoftwareRenderer::DrawPolygon3D(void* data,
	int vertexCount,
	int vertexFlag,
	Texture* texture,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	if (SetupPolygonRenderer(modelMatrix, normalMatrix)) {
		polygonRenderer.DrawPolygon3D(
			(VertexAttribute*)data, vertexCount, vertexFlag, texture);
	}
}
void SoftwareRenderer::DrawSceneLayer3D(void* layer,
	int sx,
	int sy,
	int sw,
	int sh,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	if (SetupPolygonRenderer(modelMatrix, normalMatrix)) {
		polygonRenderer.DrawSceneLayer3D((SceneLayer*)layer, sx, sy, sw, sh);
	}
}
void SoftwareRenderer::DrawModel(void* model,
	Uint16 animation,
	Uint32 frame,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	if (SetupPolygonRenderer(modelMatrix, normalMatrix)) {
		polygonRenderer.DrawModel((IModel*)model, animation, frame);
	}
}
void SoftwareRenderer::DrawModelSkinned(void* model,
	Uint16 armature,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	if (SetupPolygonRenderer(modelMatrix, normalMatrix)) {
		polygonRenderer.DrawModelSkinned((IModel*)model, armature);
	}
}
void SoftwareRenderer::DrawVertexBuffer(Uint32 vertexBufferIndex,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	if (Graphics::CurrentScene3D < 0 || vertexBufferIndex < 0 ||
		vertexBufferIndex >= MAX_VERTEX_BUFFERS) {
		return;
	}

	Scene3D* scene = &Graphics::Scene3Ds[Graphics::CurrentScene3D];
	if (!scene->Initialized) {
		return;
	}

	VertexBuffer* vertexBuffer = Graphics::VertexBuffers[vertexBufferIndex];
	if (!vertexBuffer || !vertexBuffer->FaceCount || !vertexBuffer->VertexCount) {
		return;
	}

	PolygonRenderer& rend = polygonRenderer;
	rend.ScenePtr = scene;
	rend.VertexBuf = vertexBuffer;
	rend.DoProjection = true;
	rend.DoClipping = true;
	rend.ViewMatrix = &scene->ViewMatrix;
	rend.ProjectionMatrix = &scene->ProjectionMatrix;
	rend.ModelMatrix = modelMatrix;
	rend.NormalMatrix = normalMatrix;
	rend.DrawMode = scene->DrawMode;
	rend.CurrentColor = GetBlendColor();
	rend.DrawVertexBuffer();
}

PixelFunction SoftwareRenderer::GetPixelFunction(int blendFlag) {
	if (blendFlag & BlendFlag_TINT_BIT) {
		CurrentPixelFunction = PixelTintFunctions[blendFlag & BlendFlag_MODE_MASK];
	}
	else {
		CurrentPixelFunction = PixelNoFiltFunctions[blendFlag & BlendFlag_MODE_MASK];
	}

	if (DotMaskH || DotMaskV) {
		if (DotMaskH && DotMaskV) {
			return SoftwareRenderer::PixelDotMaskHV;
		}
		else if (DotMaskH) {
			return SoftwareRenderer::PixelDotMaskH;
		}
		else if (DotMaskV) {
			return SoftwareRenderer::PixelDotMaskV;
		}
	}
	else if (UseStencil) {
		return SoftwareRenderer::PixelStencil;
	}

	return CurrentPixelFunction;
}

static void DoLineStroke(int dst_x1,
	int dst_y1,
	int dst_x2,
	int dst_y2,
	PixelFunction pixelFunction,
	Uint32 col,
	BlendState& blendState,
	int* multTableAt,
	int* multSubTableAt,
	Uint32* dstPx,
	Uint32 dstStride) {
	int dx = Math::Abs(dst_x2 - dst_x1), sx = dst_x1 < dst_x2 ? 1 : -1;
	int dy = Math::Abs(dst_y2 - dst_y1), sy = dst_y1 < dst_y2 ? 1 : -1;

	if (dy == 0) {
		int dst_strideY = dst_y1 * dstStride;
		while (dst_x1 < dst_x2) {
			pixelFunction((Uint32*)&col,
				&dstPx[dst_x1 + dst_strideY],
				blendState,
				multTableAt,
				multSubTableAt);
			dst_x1++;
		}
		return;
	}
	else if (dx == 0) {
		int dst_strideY = dst_y1 * dstStride;
		while (dst_y1 < dst_y2) {
			pixelFunction((Uint32*)&col,
				&dstPx[dst_x1 + dst_strideY],
				blendState,
				multTableAt,
				multSubTableAt);
			dst_strideY += dstStride;
			dst_y1++;
		}
		return;
	}

	int err = (dx > dy ? dx : -dy) / 2, e2;
	while (true) {
		pixelFunction((Uint32*)&col,
			&dstPx[dst_x1 + dst_y1 * dstStride],
			blendState,
			multTableAt,
			multSubTableAt);
		if (dst_x1 == dst_x2 && dst_y1 == dst_y2) {
			break;
		}
		e2 = err;
		if (e2 > -dx) {
			err -= dy;
			dst_x1 += sx;
		}
		if (e2 < dy) {
			err += dx;
			dst_y1 += sy;
		}
	}
}
static void DoLineStrokeBounded(int dst_x1,
	int dst_y1,
	int dst_x2,
	int dst_y2,
	int minX,
	int maxX,
	int minY,
	int maxY,
	PixelFunction pixelFunction,
	Uint32 col,
	BlendState& blendState,
	int* multTableAt,
	int* multSubTableAt,
	Uint32* dstPx,
	Uint32 dstStride) {
	int dx = Math::Abs(dst_x2 - dst_x1), sx = dst_x1 < dst_x2 ? 1 : -1;
	int dy = Math::Abs(dst_y2 - dst_y1), sy = dst_y1 < dst_y2 ? 1 : -1;
	int err = (dx > dy ? dx : -dy) / 2, e2;

	while (true) {
		if (dst_x1 >= minX && dst_y1 >= minY && dst_x1 < maxX && dst_y1 < maxY) {
			pixelFunction((Uint32*)&col,
				&dstPx[dst_x1 + dst_y1 * dstStride],
				blendState,
				multTableAt,
				multSubTableAt);
		}
		if (dst_x1 == dst_x2 && dst_y1 == dst_y2) {
			break;
		}
		e2 = err;
		if (e2 > -dx) {
			err -= dy;
			dst_x1 += sx;
		}
		if (e2 < dy) {
			err += dx;
			dst_y1 += sy;
		}
	}
}

void SoftwareRenderer::SetLineWidth(float n) {}
void SoftwareRenderer::StrokeLine(float x1, float y1, float x2, float y2) {
	int x = 0, y = 0;
	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	int dst_x1 = x + x1;
	int dst_y1 = y + y1;
	int dst_x2 = x + x2;
	int dst_y2 = y + y2;

	int minX, maxX, minY, maxY;
	GetClipRegion(minX, minY, maxX, maxY);
	if (!CheckClipRegion(minX, minY, maxX, maxY)) {
		return;
	}

	BlendState blendState = GetBlendState();
	if (!AlterBlendState(blendState)) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SetTintFunction(blendFlag);
	}

	int* multTableAt = &MultTable[opacity << 8];
	int* multSubTableAt = &MultSubTable[opacity << 8];

	PixelFunction pixelFunction = GetPixelFunction(blendFlag);

	DoLineStrokeBounded(dst_x1,
		dst_y1,
		dst_x2,
		dst_y2,
		minX,
		maxX,
		minY,
		maxY,
		pixelFunction,
		ColRGB,
		blendState,
		multTableAt,
		multSubTableAt,
		dstPx,
		dstStride);
}
void SoftwareRenderer::StrokeCircle(float x, float y, float rad, float thickness) {
	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	if (thickness > 1.0) {
		StrokeThickCircle(x, y, rad, thickness);
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	int dst_x1 = x - rad - 1;
	int dst_y1 = y - rad - 1;
	int dst_x2 = x + rad + 1;
	int dst_y2 = y + rad + 1;

	int clip_x1, clip_y1, clip_x2, clip_y2;
	GetClipRegion(clip_x1, clip_y1, clip_x2, clip_y2);
	if (!CheckClipRegion(clip_x1, clip_y1, clip_x2, clip_y2)) {
		return;
	}

	if (dst_x1 < clip_x1) {
		dst_x1 = clip_x1;
	}
	if (dst_y1 < clip_y1) {
		dst_y1 = clip_y1;
	}
	if (dst_x2 > clip_x2) {
		dst_x2 = clip_x2;
	}
	if (dst_y2 > clip_y2) {
		dst_y2 = clip_y2;
	}

	if (dst_x2 < 0 || dst_y2 < 0 || dst_x1 >= dst_x2 || dst_y1 >= dst_y2) {
		return;
	}

	BlendState blendState = GetBlendState();
	if (!AlterBlendState(blendState)) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;

#define DRAW_POINT(our_x, our_y) \
	if ((our_x) >= dst_x1 && (our_x) < dst_x2 && (our_y) >= dst_y1 && (our_y) < dst_y2) { \
		int dst_strideY = (our_y) * dstStride; \
		pixelFunction((Uint32*)&col, \
			&dstPx[(our_x) + dst_strideY], \
			blendState, \
			multTableAt, \
			multSubTableAt); \
	}

	Uint32 col = ColRGB;

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SetTintFunction(blendFlag);
	}

	int* multTableAt = &MultTable[opacity << 8];
	int* multSubTableAt = &MultSubTable[opacity << 8];

	PixelFunction pixelFunction = GetPixelFunction(blendFlag);

	int ccx = x, ccy = y;
	int bx = 0, by = rad;
	int bd = 3 - 2 * rad;
	while (bx <= by) {
		DRAW_POINT(ccx + bx, ccy - by);
		DRAW_POINT(ccx - bx, ccy - by);
		DRAW_POINT(ccx + by, ccy - bx);
		DRAW_POINT(ccx - by, ccy - bx);
		ccy--;
		DRAW_POINT(ccx + bx, ccy + by);
		DRAW_POINT(ccx - bx, ccy + by);
		DRAW_POINT(ccx + by, ccy + bx);
		DRAW_POINT(ccx - by, ccy + bx);
		ccy++;
		if (bd <= 0) {
			bd += 4 * bx + 6;
		}
		else {
			bd += 4 * (bx - by) + 10;
			by--;
		}
		bx++;
	}

#undef DRAW_POINT
}
void SoftwareRenderer::InitContour(Contour* contourBuffer, int dst_y1, int scanLineCount) {
	Contour* contourPtr = &contourBuffer[dst_y1];
	while (scanLineCount--) {
		contourPtr->MinX = 0x7FFFFFFF;
		contourPtr->MaxX = -1;
		contourPtr++;
	}
}
void SoftwareRenderer::RasterizeCircle(int ccx,
	int ccy,
	int dst_x1,
	int dst_y1,
	int dst_x2,
	int dst_y2,
	float rad,
	Contour* contourBuffer) {
#define SEEK_MIN(our_x, our_y) \
	if (our_y >= dst_y1 && our_y < dst_y2 && our_x < (cont = &contourBuffer[our_y])->MinX) \
		cont->MinX = our_x<dst_x1 ? dst_x1 : our_x>(dst_x2 - 1) ? dst_x2 - 1 : our_x;
#define SEEK_MAX(our_x, our_y) \
	if (our_y >= dst_y1 && our_y < dst_y2 && our_x > (cont = &contourBuffer[our_y])->MaxX) \
		cont->MaxX = our_x<dst_x1 ? dst_x1 : our_x>(dst_x2 - 1) ? dst_x2 - 1 : our_x;

	Contour* cont;
	int bx = 0, by = rad;
	int bd = 3 - 2 * rad;
	while (bx <= by) {
		SEEK_MAX(ccx + bx, ccy - by);
		SEEK_MIN(ccx - bx, ccy - by);
		SEEK_MAX(ccx + by, ccy - bx);
		SEEK_MIN(ccx - by, ccy - bx);
		ccy--;
		SEEK_MAX(ccx + bx, ccy + by);
		SEEK_MIN(ccx - bx, ccy + by);
		SEEK_MAX(ccx + by, ccy + bx);
		SEEK_MIN(ccx - by, ccy + bx);
		ccy++;
		if (bd <= 0) {
			bd += 4 * bx + 6;
		}
		else {
			bd += 4 * (bx - by) + 10;
			by--;
		}
		bx++;
	}

#undef SEEK_MIN
#undef SEEK_MAX
}
void SoftwareRenderer::StrokeThickCircle(float x, float y, float rad, float thickness) {
	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	float radA = rad + (thickness * 0.5);
	float radB = radA - thickness;
	if (radB <= 0.0) {
		SoftwareRenderer::FillCircle(x, y, rad);
		return;
	}

	rad = radA;

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	int dst_x1 = x - rad - 1;
	int dst_y1 = y - rad - 1;
	int dst_x2 = x + rad + 1;
	int dst_y2 = y + rad + 1;

	int clip_x1, clip_y1, clip_x2, clip_y2;
	GetClipRegion(clip_x1, clip_y1, clip_x2, clip_y2);
	if (!CheckClipRegion(clip_x1, clip_y1, clip_x2, clip_y2)) {
		return;
	}

	if (dst_x1 < clip_x1) {
		dst_x1 = clip_x1;
	}
	if (dst_y1 < clip_y1) {
		dst_y1 = clip_y1;
	}
	if (dst_x2 > clip_x2) {
		dst_x2 = clip_x2;
	}
	if (dst_y2 > clip_y2) {
		dst_y2 = clip_y2;
	}

	if (dst_x2 < 0 || dst_y2 < 0 || dst_x1 >= dst_x2 || dst_y1 >= dst_y2) {
		return;
	}

	int in_dst_x1 = x - radB - 1;
	int in_dst_y1 = y - radB - 1;
	int in_dst_x2 = x + radB + 1;
	int in_dst_y2 = y + radB + 1;

	if (in_dst_x1 < clip_x1) {
		in_dst_x1 = clip_x1;
	}
	if (in_dst_y1 < clip_y1) {
		in_dst_y1 = clip_y1;
	}
	if (in_dst_x2 > clip_x2) {
		in_dst_x2 = clip_x2;
	}
	if (in_dst_y2 > clip_y2) {
		in_dst_y2 = clip_y2;
	}

	if (in_dst_x2 < 0 || in_dst_y2 < 0 || in_dst_x1 >= dst_x2 || in_dst_y1 >= dst_y2) {
		return;
	}

	BlendState blendState = GetBlendState();
	if (!AlterBlendState(blendState)) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;

	Contour* contourBufferA = ContourBuffer;
	Contour contourBufferB[MAX_FRAMEBUFFER_HEIGHT];

	InitContour(contourBufferA, dst_y1, dst_y2 - dst_y1 + 1);
	InitContour(contourBufferB, in_dst_y1, in_dst_y2 - in_dst_y1 + 1);

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SetTintFunction(blendFlag);
	}

	int* multTableAt = &MultTable[opacity << 8];
	int* multSubTableAt = &MultSubTable[opacity << 8];

	Uint32 col = ColRGB;

	RasterizeCircle(x, y, dst_x1, dst_y1, dst_x2, dst_y2, rad, contourBufferA);
	RasterizeCircle(x, y, in_dst_x1, in_dst_y1, in_dst_x2, in_dst_y2, radB, contourBufferB);

	int dst_strideY = dst_y1 * dstStride;

	if (!UseStencil &&
		((blendFlag & (BlendFlag_MODE_MASK | BlendFlag_TINT_BIT)) == BlendFlag_OPAQUE)) {
		for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++, dst_strideY += dstStride) {
			Contour contourA = contourBufferA[dst_y];
			if (contourA.MaxX < contourA.MinX) {
				continue;
			}

			if (dst_y <= in_dst_y1 || dst_y >= in_dst_y2 - 1) {
				Memory::Memset4(&dstPx[contourA.MinX + dst_strideY],
					col,
					contourA.MaxX - contourA.MinX);
				continue;
			}

			Contour contourB = contourBufferB[dst_y];
			if (contourB.MinX == 0x7FFFFFFF) {
				contourB.MinX = 0;
			}
			else if (contourB.MinX < contourA.MinX) {
				continue;
			}
			if (contourB.MaxX == -1) {
				contourB.MaxX = dst_x2;
			}
			else if (contourB.MaxX >= contourA.MaxX) {
				continue;
			}
			if (contourB.MaxX < contourB.MinX) {
				continue;
			}

			Memory::Memset4(&dstPx[contourA.MinX + dst_strideY],
				col,
				contourB.MinX - contourA.MinX);
			Memory::Memset4(&dstPx[contourB.MaxX + dst_strideY],
				col,
				contourA.MaxX - contourB.MaxX);
		}
	}
	else {
		PixelFunction pixelFunction = GetPixelFunction(blendFlag);

		for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++, dst_strideY += dstStride) {
			Contour contourA = contourBufferA[dst_y];
			if (contourA.MaxX < contourA.MinX) {
				continue;
			}

			if (dst_y <= in_dst_y1 || dst_y >= in_dst_y2 - 1) {
				for (int dst_x = contourA.MinX; dst_x < contourA.MaxX; dst_x++) {
					pixelFunction((Uint32*)&col,
						&dstPx[dst_x + dst_strideY],
						blendState,
						multTableAt,
						multSubTableAt);
				}
				continue;
			}

			Contour contourB = contourBufferB[dst_y];
			if (contourB.MinX == 0x7FFFFFFF) {
				contourB.MinX = 0;
			}
			else if (contourB.MinX < contourA.MinX) {
				continue;
			}
			if (contourB.MaxX == -1) {
				contourB.MaxX = dst_x2;
			}
			else if (contourB.MaxX >= contourA.MaxX) {
				continue;
			}
			if (contourB.MaxX < contourB.MinX) {
				continue;
			}

			for (int dst_x = contourA.MinX; dst_x < contourB.MinX; dst_x++) {
				pixelFunction((Uint32*)&col,
					&dstPx[dst_x + dst_strideY],
					blendState,
					multTableAt,
					multSubTableAt);
			}
			for (int dst_x = contourB.MaxX; dst_x < contourA.MaxX; dst_x++) {
				pixelFunction((Uint32*)&col,
					&dstPx[dst_x + dst_strideY],
					blendState,
					multTableAt,
					multSubTableAt);
			}
		}
	}
}
void SoftwareRenderer::StrokeEllipse(float x, float y, float w, float h) {}
void SoftwareRenderer::StrokeRectangle(float x, float y, float w, float h) {
	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	int dst_x1 = x;
	int dst_y1 = y;
	int dst_x2 = x + w;
	int dst_y2 = y + h;

	int clip_x1, clip_y1, clip_x2, clip_y2;
	GetClipRegion(clip_x1, clip_y1, clip_x2, clip_y2);
	if (!CheckClipRegion(clip_x1, clip_y1, clip_x2, clip_y2)) {
		return;
	}

	if (dst_x1 < clip_x1) {
		dst_x1 = clip_x1;
	}
	if (dst_y1 < clip_y1) {
		dst_y1 = clip_y1;
	}
	if (dst_x2 > clip_x2) {
		dst_x2 = clip_x2;
	}
	if (dst_y2 > clip_y2) {
		dst_y2 = clip_y2;
	}

	if (dst_x2 < 0 || dst_y2 < 0 || dst_x1 >= dst_x2 || dst_y1 >= dst_y2) {
		return;
	}

	BlendState blendState = GetBlendState();
	if (!AlterBlendState(blendState)) {
		return;
	}

	Uint32 col = ColRGB;
	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SetTintFunction(blendFlag);
	}

	int* multTableAt = &MultTable[opacity << 8];
	int* multSubTableAt = &MultSubTable[opacity << 8];

	PixelFunction pixelFunction = GetPixelFunction(blendFlag);

	// top
	DoLineStroke(dst_x1,
		dst_y1,
		dst_x2,
		dst_y1,
		pixelFunction,
		col,
		blendState,
		multTableAt,
		multSubTableAt,
		dstPx,
		dstStride);

	// bottom
	DoLineStroke(dst_x1,
		dst_y2 - 1,
		dst_x2,
		dst_y2 - 1,
		pixelFunction,
		col,
		blendState,
		multTableAt,
		multSubTableAt,
		dstPx,
		dstStride);

	// left
	DoLineStroke(dst_x1,
		dst_y1 + 1,
		dst_x1,
		dst_y2 - 1,
		pixelFunction,
		col,
		blendState,
		multTableAt,
		multSubTableAt,
		dstPx,
		dstStride);

	// right
	DoLineStroke(dst_x2 - 1,
		dst_y1 + 1,
		dst_x2 - 1,
		dst_y2 - 1,
		pixelFunction,
		col,
		blendState,
		multTableAt,
		multSubTableAt,
		dstPx,
		dstStride);
}

void SoftwareRenderer::FillCircle(float x, float y, float rad) {
	// just checks to see if the pixel is within a radius range,
	// uses a bounding box constructed by the diameter

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	int dst_x1 = x - rad - 1;
	int dst_y1 = y - rad - 1;
	int dst_x2 = x + rad + 1;
	int dst_y2 = y + rad + 1;

	int clip_x1, clip_y1, clip_x2, clip_y2;
	GetClipRegion(clip_x1, clip_y1, clip_x2, clip_y2);
	if (!CheckClipRegion(clip_x1, clip_y1, clip_x2, clip_y2)) {
		return;
	}

	if (dst_x1 < clip_x1) {
		dst_x1 = clip_x1;
	}
	if (dst_y1 < clip_y1) {
		dst_y1 = clip_y1;
	}
	if (dst_x2 > clip_x2) {
		dst_x2 = clip_x2;
	}
	if (dst_y2 > clip_y2) {
		dst_y2 = clip_y2;
	}

	if (dst_x2 < 0 || dst_y2 < 0 || dst_x1 >= dst_x2 || dst_y1 >= dst_y2) {
		return;
	}

	BlendState blendState = GetBlendState();
	if (!AlterBlendState(blendState)) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;

	InitContour(ContourBuffer, dst_y1, dst_y2 - dst_y1 + 1);

#define SEEK_MIN(our_x, our_y) \
	if (our_y >= dst_y1 && our_y < dst_y2 && our_x < (cont = &ContourBuffer[our_y])->MinX) \
		cont->MinX = our_x<dst_x1 ? dst_x1 : our_x>(dst_x2 - 1) ? dst_x2 - 1 : our_x;
#define SEEK_MAX(our_x, our_y) \
	if (our_y >= dst_y1 && our_y < dst_y2 && our_x > (cont = &ContourBuffer[our_y])->MaxX) \
		cont->MaxX = our_x<dst_x1 ? dst_x1 : our_x>(dst_x2 - 1) ? dst_x2 - 1 : our_x;

	RasterizeCircle(x, y, dst_x1, dst_y1, dst_x2, dst_y2, rad, ContourBuffer);

	Uint32 col = ColRGB;

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SetTintFunction(blendFlag);
	}

	int* multTableAt = &MultTable[opacity << 8];
	int* multSubTableAt = &MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;

	if (!UseStencil &&
		((blendFlag & (BlendFlag_MODE_MASK | BlendFlag_TINT_BIT)) == BlendFlag_OPAQUE)) {
		for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
			Contour contour = ContourBuffer[dst_y];
			if (contour.MaxX < contour.MinX) {
				dst_strideY += dstStride;
				continue;
			}
			int dst_min_x = contour.MinX;
			if (dst_min_x < dst_x1) {
				dst_min_x = dst_x1;
			}
			int dst_max_x = contour.MaxX;
			if (dst_max_x > dst_x2 - 1) {
				dst_max_x = dst_x2 - 1;
			}

			Memory::Memset4(
				&dstPx[dst_min_x + dst_strideY], col, dst_max_x - dst_min_x);
			dst_strideY += dstStride;
		}
	}
	else {
		PixelFunction pixelFunction = GetPixelFunction(blendFlag);

		for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
			Contour contour = ContourBuffer[dst_y];
			if (contour.MaxX < contour.MinX) {
				dst_strideY += dstStride;
				continue;
			}
			for (int dst_x = contour.MinX >= dst_x1 ? contour.MinX : dst_x1;
				dst_x < contour.MaxX && dst_x < dst_x2;
				dst_x++) {
				pixelFunction((Uint32*)&col,
					&dstPx[dst_x + dst_strideY],
					blendState,
					multTableAt,
					multSubTableAt);
			}
			dst_strideY += dstStride;
		}
	}

#undef SEEK_MIN
#undef SEEK_MAX
}
void SoftwareRenderer::FillEllipse(float x, float y, float w, float h) {}
void SoftwareRenderer::FillRectangle(float x, float y, float w, float h) {
	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;

	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	int dst_x1 = x;
	int dst_y1 = y;
	int dst_x2 = x + w;
	int dst_y2 = y + h;

	int clip_x1, clip_y1, clip_x2, clip_y2;
	GetClipRegion(clip_x1, clip_y1, clip_x2, clip_y2);
	if (!CheckClipRegion(clip_x1, clip_y1, clip_x2, clip_y2)) {
		return;
	}

	if (dst_x1 < clip_x1) {
		dst_x1 = clip_x1;
	}
	if (dst_y1 < clip_y1) {
		dst_y1 = clip_y1;
	}
	if (dst_x2 > clip_x2) {
		dst_x2 = clip_x2;
	}
	if (dst_y2 > clip_y2) {
		dst_y2 = clip_y2;
	}

	if (dst_x2 < 0 || dst_y2 < 0 || dst_x1 >= dst_x2 || dst_y1 >= dst_y2) {
		return;
	}

	BlendState blendState = GetBlendState();
	if (!AlterBlendState(blendState)) {
		return;
	}

	Uint32 col = ColRGB;
	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SetTintFunction(blendFlag);
	}

	int* multTableAt = &MultTable[opacity << 8];
	int* multSubTableAt = &MultSubTable[opacity << 8];
	int dst_strideY = dst_y1 * dstStride;

	if (!UseStencil &&
		((blendFlag & (BlendFlag_MODE_MASK | BlendFlag_TINT_BIT)) == BlendFlag_OPAQUE)) {
		for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
			Memory::Memset4(&dstPx[dst_x1 + dst_strideY], col, dst_x2 - dst_x1);
			dst_strideY += dstStride;
		}
	}
	else {
		PixelFunction pixelFunction = GetPixelFunction(blendFlag);

		for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
			for (int dst_x = dst_x1; dst_x < dst_x2; dst_x++) {
				pixelFunction((Uint32*)&col,
					&dstPx[dst_x + dst_strideY],
					blendState,
					multTableAt,
					multSubTableAt);
			}
			dst_strideY += dstStride;
		}
	}
}
void SoftwareRenderer::FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {
	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	BlendState blendState = GetBlendState();
	if (!AlterBlendState(blendState)) {
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	int x = 0, y = 0;

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	Vector2 vectors[3];
	vectors[0].X = ((int)x1 + x) << 16;
	vectors[0].Y = ((int)y1 + y) << 16;
	vectors[1].X = ((int)x2 + x) << 16;
	vectors[1].Y = ((int)y2 + y) << 16;
	vectors[2].X = ((int)x3 + x) << 16;
	vectors[2].Y = ((int)y3 + y) << 16;
	PolygonRasterizer::DrawBasic(vectors, ColRGB, 3, blendState);
}
void SoftwareRenderer::FillTriangleBlend(float x1,
	float y1,
	float x2,
	float y2,
	float x3,
	float y3,
	int c1,
	int c2,
	int c3) {
	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	BlendState blendState = GetBlendState();
	if (!AlterBlendState(blendState)) {
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	int x = 0, y = 0;

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	int colors[3];
	Vector2 vectors[3];
	vectors[0].X = ((int)x1 + x) << 16;
	vectors[0].Y = ((int)y1 + y) << 16;
	colors[0] = ColorUtils::Multiply(c1, GetBlendColor());
	vectors[1].X = ((int)x2 + x) << 16;
	vectors[1].Y = ((int)y2 + y) << 16;
	colors[1] = ColorUtils::Multiply(c2, GetBlendColor());
	vectors[2].X = ((int)x3 + x) << 16;
	vectors[2].Y = ((int)y3 + y) << 16;
	colors[2] = ColorUtils::Multiply(c3, GetBlendColor());
	PolygonRasterizer::DrawBasicBlend(vectors, colors, 3, blendState);
}
void SoftwareRenderer::FillQuad(float x1,
	float y1,
	float x2,
	float y2,
	float x3,
	float y3,
	float x4,
	float y4) {
	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	BlendState blendState = GetBlendState();
	if (!AlterBlendState(blendState)) {
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	int x = 0, y = 0;

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	Vector2 vectors[4];
	vectors[0].X = ((int)x1 + x) << 16;
	vectors[0].Y = ((int)y1 + y) << 16;
	vectors[1].X = ((int)x2 + x) << 16;
	vectors[1].Y = ((int)y2 + y) << 16;
	vectors[2].X = ((int)x3 + x) << 16;
	vectors[2].Y = ((int)y3 + y) << 16;
	vectors[3].X = ((int)x4 + x) << 16;
	vectors[3].Y = ((int)y4 + y) << 16;
	PolygonRasterizer::DrawBasic(vectors, ColRGB, 4, blendState);
}
void SoftwareRenderer::FillQuadBlend(float x1,
	float y1,
	float x2,
	float y2,
	float x3,
	float y3,
	float x4,
	float y4,
	int c1,
	int c2,
	int c3,
	int c4) {
	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	BlendState blendState = GetBlendState();
	if (!AlterBlendState(blendState)) {
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	int x = 0, y = 0;

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	int colors[4];
	Vector2 vectors[4];
	vectors[0].X = ((int)x1 + x) << 16;
	vectors[0].Y = ((int)y1 + y) << 16;
	colors[0] = ColorUtils::Multiply(c1, GetBlendColor());
	vectors[1].X = ((int)x2 + x) << 16;
	vectors[1].Y = ((int)y2 + y) << 16;
	colors[1] = ColorUtils::Multiply(c2, GetBlendColor());
	vectors[2].X = ((int)x3 + x) << 16;
	vectors[2].Y = ((int)y3 + y) << 16;
	colors[2] = ColorUtils::Multiply(c3, GetBlendColor());
	vectors[3].X = ((int)x4 + x) << 16;
	vectors[3].Y = ((int)y4 + y) << 16;
	colors[3] = ColorUtils::Multiply(c4, GetBlendColor());
	PolygonRasterizer::DrawBasicBlend(vectors, colors, 4, blendState);
}
void SoftwareRenderer::DrawShapeTextured(Texture* texturePtr,
	unsigned numPoints,
	float* px,
	float* py,
	int* pc,
	float* pu,
	float* pv) {
	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	BlendState blendState = GetBlendState();
	if (!AlterBlendState(blendState)) {
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	int x = 0, y = 0;

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	int colors[MAX_POLYGON_VERTICES];
	Vector3 vectors[MAX_POLYGON_VERTICES];
	Vector2 uv[MAX_POLYGON_VERTICES];

	if (numPoints > MAX_POLYGON_VERTICES) {
		numPoints = MAX_POLYGON_VERTICES;
	}

	for (unsigned i = 0; i < numPoints; i++) {
		vectors[i].X = ((int)px[i] + x) << 16;
		vectors[i].Y = ((int)py[i] + y) << 16;
		vectors[i].Z = FP16_TO(1.0f);
		colors[i] = ColorUtils::Multiply(pc[i], GetBlendColor());
		uv[i].X = ((int)pu[i]) << 16;
		uv[i].Y = ((int)pv[i]) << 16;
	}

	PolygonRasterizer::DrawBlendPerspective(
		texturePtr, vectors, uv, colors, numPoints, blendState);
}
void SoftwareRenderer::DrawTriangleTextured(Texture* texturePtr,
	float x1,
	float y1,
	float x2,
	float y2,
	float x3,
	float y3,
	int c1,
	int c2,
	int c3,
	float u1,
	float v1,
	float u2,
	float v2,
	float u3,
	float v3) {
	float px[3];
	float py[3];
	float pu[3];
	float pv[3];
	int pc[3];

	px[0] = x1;
	py[0] = y1;
	pu[0] = u1;
	pv[0] = v1;
	pc[0] = c1;

	px[1] = x2;
	py[1] = y2;
	pu[1] = u2;
	pv[1] = v2;
	pc[1] = c2;

	px[2] = x3;
	py[2] = y3;
	pu[2] = u3;
	pv[2] = v3;
	pc[2] = c3;

	DrawShapeTextured(texturePtr, 3, px, py, pc, pu, pv);
}
void SoftwareRenderer::DrawQuadTextured(Texture* texturePtr,
	float x1,
	float y1,
	float x2,
	float y2,
	float x3,
	float y3,
	float x4,
	float y4,
	int c1,
	int c2,
	int c3,
	int c4,
	float u1,
	float v1,
	float u2,
	float v2,
	float u3,
	float v3,
	float u4,
	float v4) {
	float px[4];
	float py[4];
	float pu[4];
	float pv[4];
	int pc[4];

	px[0] = x1;
	py[0] = y1;
	pu[0] = u1;
	pv[0] = v1;
	pc[0] = c1;

	px[1] = x2;
	py[1] = y2;
	pu[1] = u2;
	pv[1] = v2;
	pc[1] = c2;

	px[2] = x3;
	py[2] = y3;
	pu[2] = u3;
	pv[2] = v3;
	pc[2] = c3;

	px[3] = x4;
	py[3] = y4;
	pu[3] = u4;
	pv[3] = v4;
	pc[3] = c4;

	DrawShapeTextured(texturePtr, 4, px, py, pc, pu, pv);
}

void DrawSpriteImage(Texture* texture,
	int x,
	int y,
	int w,
	int h,
	int sx,
	int sy,
	int flipFlag,
	unsigned paletteID,
	BlendState blendState) {
	Uint32* srcPx = (Uint32*)texture->Pixels;
	Uint32 srcStride = texture->Width;
	Uint32* srcPxLine;

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;
	Uint32* dstPxLine;

	int src_x1 = sx;
	int src_y1 = sy;
	int src_x2 = sx + w - 1;
	int src_y2 = sy + h - 1;

	int dst_x1 = x;
	int dst_y1 = y;
	int dst_x2 = x + w;
	int dst_y2 = y + h;

	if (!Graphics::TextureBlend) {
		blendState.Mode = BlendMode_NORMAL;
		blendState.Opacity = 0xFF;
	}

	if (!SoftwareRenderer::AlterBlendState(blendState)) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;

	int clip_x1, clip_y1, clip_x2, clip_y2;
	GetClipRegion(clip_x1, clip_y1, clip_x2, clip_y2);
	if (!CheckClipRegion(clip_x1, clip_y1, clip_x2, clip_y2)) {
		return;
	}

	if (dst_x2 > clip_x2) {
		dst_x2 = clip_x2;
	}
	if (dst_y2 > clip_y2) {
		dst_y2 = clip_y2;
	}

	if (dst_x1 < clip_x1) {
		src_x1 += clip_x1 - dst_x1;
		src_x2 -= clip_x1 - dst_x1;
		dst_x1 = clip_x1;
	}
	if (dst_y1 < clip_y1) {
		src_y1 += clip_y1 - dst_y1;
		src_y2 -= clip_y1 - dst_y1;
		dst_y1 = clip_y1;
	}

	if (dst_x2 < 0 || dst_y2 < 0 || dst_x1 >= dst_x2 || dst_y1 >= dst_y2) {
		return;
	}

#define DEFORM_X \
	{ \
		dst_x += *deformValues; \
		if (dst_x < clip_x1) { \
			dst_x -= *deformValues; \
			continue; \
		} \
		if (dst_x >= clip_x2) { \
			dst_x -= *deformValues; \
			continue; \
		} \
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

#define DRAW_PLACEPIXEL() \
	if ((color = srcPxLine[src_x]) & 0xFF000000U) \
		pixelFunction(&color, &dstPxLine[dst_x], blendState, multTableAt, multSubTableAt);
#define DRAW_PLACEPIXEL_PAL() \
	if ((color = srcPxLine[src_x]) && (index[color] & 0xFF000000U)) \
		pixelFunction(&index[color], \
			&dstPxLine[dst_x], \
			blendState, \
			multTableAt, \
			multSubTableAt);

#define DRAW_NOFLIP(placePixelMacro) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		srcPxLine = srcPx + src_strideY; \
		dstPxLine = dstPx + dst_strideY; \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		if (SoftwareRenderer::UseSpriteDeform) \
			for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; \
				dst_x++, src_x++) { \
				DEFORM_X; \
				placePixelMacro() dst_x -= *deformValues; \
			} \
		else \
			for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; \
				dst_x++, src_x++) { \
				placePixelMacro() \
			} \
\
		dst_strideY += dstStride; \
		src_strideY += srcStride; \
		deformValues++; \
	}
#define DRAW_FLIPX(placePixelMacro) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		srcPxLine = srcPx + src_strideY; \
		dstPxLine = dstPx + dst_strideY; \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		if (SoftwareRenderer::UseSpriteDeform) \
			for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; \
				dst_x++, src_x--) { \
				DEFORM_X; \
				placePixelMacro() dst_x -= *deformValues; \
			} \
		else \
			for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; \
				dst_x++, src_x--) { \
				placePixelMacro() \
			} \
		dst_strideY += dstStride; \
		src_strideY += srcStride; \
		deformValues++; \
	}
#define DRAW_FLIPY(placePixelMacro) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		srcPxLine = srcPx + src_strideY; \
		dstPxLine = dstPx + dst_strideY; \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		if (SoftwareRenderer::UseSpriteDeform) \
			for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; \
				dst_x++, src_x++) { \
				DEFORM_X; \
				placePixelMacro() dst_x -= *deformValues; \
			} \
		else \
			for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; \
				dst_x++, src_x++) { \
				placePixelMacro() \
			} \
		dst_strideY += dstStride; \
		src_strideY -= srcStride; \
		deformValues++; \
	}
#define DRAW_FLIPXY(placePixelMacro) \
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
		srcPxLine = srcPx + src_strideY; \
		dstPxLine = dstPx + dst_strideY; \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		if (SoftwareRenderer::UseSpriteDeform) \
			for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; \
				dst_x++, src_x--) { \
				DEFORM_X; \
				placePixelMacro() dst_x -= *deformValues; \
			} \
		else \
			for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; \
				dst_x++, src_x--) { \
				placePixelMacro() \
			} \
		dst_strideY += dstStride; \
		src_strideY -= srcStride; \
		deformValues++; \
	}

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	Uint32 color;
	Uint32* index = nullptr;
	int dst_strideY, src_strideY;
	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	Sint32* deformValues = &SoftwareRenderer::SpriteDeformBuffer[dst_y1];

	if (Graphics::UsePalettes && texture->Paletted) {
		if (!Graphics::UsePaletteIndexLines) {
			index = &Graphics::PaletteColors[paletteID][0];
		}

		switch (flipFlag) {
		case 0:
			dst_strideY = dst_y1 * dstStride;
			src_strideY = src_y1 * srcStride;
			DRAW_NOFLIP(DRAW_PLACEPIXEL_PAL);
			break;
		case 1:
			dst_strideY = dst_y1 * dstStride;
			src_strideY = src_y1 * srcStride;
			DRAW_FLIPX(DRAW_PLACEPIXEL_PAL);
			break;
		case 2:
			dst_strideY = dst_y1 * dstStride;
			src_strideY = src_y2 * srcStride;
			DRAW_FLIPY(DRAW_PLACEPIXEL_PAL);
			break;
		case 3:
			dst_strideY = dst_y1 * dstStride;
			src_strideY = src_y2 * srcStride;
			DRAW_FLIPXY(DRAW_PLACEPIXEL_PAL);
			break;
		}
	}
	else {
		switch (flipFlag) {
		case 0:
			dst_strideY = dst_y1 * dstStride;
			src_strideY = src_y1 * srcStride;
			DRAW_NOFLIP(DRAW_PLACEPIXEL);
			break;
		case 1:
			dst_strideY = dst_y1 * dstStride;
			src_strideY = src_y1 * srcStride;
			DRAW_FLIPX(DRAW_PLACEPIXEL);
			break;
		case 2:
			dst_strideY = dst_y1 * dstStride;
			src_strideY = src_y2 * srcStride;
			DRAW_FLIPY(DRAW_PLACEPIXEL);
			break;
		case 3:
			dst_strideY = dst_y1 * dstStride;
			src_strideY = src_y2 * srcStride;
			DRAW_FLIPXY(DRAW_PLACEPIXEL);
			break;
		}
	}

#undef DRAW_PLACEPIXEL
#undef DRAW_PLACEPIXEL_PAL
#undef DRAW_NOFLIP
#undef DRAW_FLIPX
#undef DRAW_FLIPY
#undef DRAW_FLIPXY
}
void DrawSpriteImageTransformed(Texture* texture,
	int x,
	int y,
	int offx,
	int offy,
	int w,
	int h,
	int sx,
	int sy,
	int sw,
	int sh,
	int flipFlag,
	int rotation,
	unsigned paletteID,
	BlendState blendState) {
	Uint32* srcPx = (Uint32*)texture->Pixels;
	Uint32 srcStride = texture->Width;

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;
	Uint32* dstPxLine;

	int src_x;
	int src_y;
	int src_x1 = sx;
	int src_y1 = sy;
	int src_x2 = sx + sw - 1;
	int src_y2 = sy + sh - 1;

	int cos = CosTable[rotation];
	int sin = SinTable[rotation];
	int rcos = CosTable[(TRIG_TABLE_SIZE - rotation + TRIG_TABLE_SIZE) & TRIG_TABLE_MASK];
	int rsin = SinTable[(TRIG_TABLE_SIZE - rotation + TRIG_TABLE_SIZE) & TRIG_TABLE_MASK];

	int _x1 = offx;
	int _y1 = offy;
	int _x2 = offx + w;
	int _y2 = offy + h;

	switch (flipFlag) {
	case 1:
		_x1 = -offx - w;
		_x2 = -offx;
		break;
	case 2:
		_y1 = -offy - h;
		_y2 = -offy;
		break;
	case 3:
		_x1 = -offx - w;
		_x2 = -offx;
		_y1 = -offy - h;
		_y2 = -offy;
	}

	int dst_x1 = _x1;
	int dst_y1 = _y1;
	int dst_x2 = _x2;
	int dst_y2 = _y2;

	if (!Graphics::TextureBlend) {
		blendState.Mode = BlendMode_NORMAL;
		blendState.Opacity = 0xFF;
	}

	if (!SoftwareRenderer::AlterBlendState(blendState)) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;

#define SET_MIN(a, b) \
	if (a > b) \
		a = b;
#define SET_MAX(a, b) \
	if (a < b) \
		a = b;

	int px, py, cx, cy;

	py = _y1;
	{
		px = _x1;
		cx = (px * cos - py * sin);
		SET_MIN(dst_x1, cx);
		SET_MAX(dst_x2, cx);
		cy = (px * sin + py * cos);
		SET_MIN(dst_y1, cy);
		SET_MAX(dst_y2, cy);

		px = _x2;
		cx = (px * cos - py * sin);
		SET_MIN(dst_x1, cx);
		SET_MAX(dst_x2, cx);
		cy = (px * sin + py * cos);
		SET_MIN(dst_y1, cy);
		SET_MAX(dst_y2, cy);
	}

	py = _y2;
	{
		px = _x1;
		cx = (px * cos - py * sin);
		SET_MIN(dst_x1, cx);
		SET_MAX(dst_x2, cx);
		cy = (px * sin + py * cos);
		SET_MIN(dst_y1, cy);
		SET_MAX(dst_y2, cy);

		px = _x2;
		cx = (px * cos - py * sin);
		SET_MIN(dst_x1, cx);
		SET_MAX(dst_x2, cx);
		cy = (px * sin + py * cos);
		SET_MIN(dst_y1, cy);
		SET_MAX(dst_y2, cy);
	}

#undef SET_MIN
#undef SET_MAX

	dst_x1 >>= TRIG_TABLE_BITS;
	dst_y1 >>= TRIG_TABLE_BITS;
	dst_x2 >>= TRIG_TABLE_BITS;
	dst_y2 >>= TRIG_TABLE_BITS;

	dst_x1 += x;
	dst_y1 += y;
	dst_x2 += x + 1;
	dst_y2 += y + 1;

	int clip_x1, clip_y1, clip_x2, clip_y2;
	GetClipRegion(clip_x1, clip_y1, clip_x2, clip_y2);
	if (!CheckClipRegion(clip_x1, clip_y1, clip_x2, clip_y2)) {
		return;
	}

	if (dst_x1 < clip_x1) {
		dst_x1 = clip_x1;
	}
	if (dst_y1 < clip_y1) {
		dst_y1 = clip_y1;
	}
	if (dst_x2 > clip_x2) {
		dst_x2 = clip_x2;
	}
	if (dst_y2 > clip_y2) {
		dst_y2 = clip_y2;
	}

	if (dst_x2 < 0 || dst_y2 < 0 || dst_x1 >= dst_x2 || dst_y1 >= dst_y2) {
		return;
	}

#define DEFORM_X \
	{ \
		dst_x += *deformValues; \
		if (dst_x < clip_x1) { \
			dst_x -= *deformValues; \
			continue; \
		} \
		if (dst_x >= clip_x2) { \
			dst_x -= *deformValues; \
			continue; \
		} \
	}

	PixelFunction pixelFunction = SoftwareRenderer::GetPixelFunction(blendFlag);

#define DRAW_PLACEPIXEL() \
	if ((color = srcPx[src_x + src_strideY]) & 0xFF000000U) \
		pixelFunction(&color, &dstPxLine[dst_x], blendState, multTableAt, multSubTableAt);
#define DRAW_PLACEPIXEL_PAL() \
	if ((color = srcPx[src_x + src_strideY]) && (index[color] & 0xFF000000U)) \
		pixelFunction(&index[color], \
			&dstPxLine[dst_x], \
			blendState, \
			multTableAt, \
			multSubTableAt);

#define DRAW_NOFLIP(placePixelMacro) \
	for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
		i_y_rsin = -i_y * rsin; \
		i_y_rcos = i_y * rcos; \
		dstPxLine = dstPx + dst_strideY; \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		if (SoftwareRenderer::UseSpriteDeform) \
			for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; \
				dst_x++, i_x++) { \
				DEFORM_X; \
				src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
				src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
				if (src_x >= _x1 && src_y >= _y1 && src_x < _x2 && src_y < _y2) { \
					src_x = (src_x1 + (src_x - _x1) * sw / w); \
					src_strideY = \
						(src_y1 + (src_y - _y1) * sh / h) * srcStride; \
					placePixelMacro(); \
				} \
				dst_x -= *deformValues; \
			} \
		else \
			for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; \
				dst_x++, i_x++) { \
				src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
				src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
				if (src_x >= _x1 && src_y >= _y1 && src_x < _x2 && src_y < _y2) { \
					src_x = (src_x1 + (src_x - _x1) * sw / w); \
					src_strideY = \
						(src_y1 + (src_y - _y1) * sh / h) * srcStride; \
					placePixelMacro(); \
				} \
			} \
		dst_strideY += dstStride; \
		deformValues++; \
	}
#define DRAW_FLIPX(placePixelMacro) \
	for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
		i_y_rsin = -i_y * rsin; \
		i_y_rcos = i_y * rcos; \
		dstPxLine = dstPx + dst_strideY; \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		if (SoftwareRenderer::UseSpriteDeform) \
			for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; \
				dst_x++, i_x++) { \
				DEFORM_X; \
				src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
				src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
				if (src_x >= _x1 && src_y >= _y1 && src_x < _x2 && src_y < _y2) { \
					src_x = (src_x2 - (src_x - _x1) * sw / w); \
					src_strideY = \
						(src_y1 + (src_y - _y1) * sh / h) * srcStride; \
					placePixelMacro(); \
				} \
				dst_x -= *deformValues; \
			} \
		else \
			for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; \
				dst_x++, i_x++) { \
				src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
				src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
				if (src_x >= _x1 && src_y >= _y1 && src_x < _x2 && src_y < _y2) { \
					src_x = (src_x2 - (src_x - _x1) * sw / w); \
					src_strideY = \
						(src_y1 + (src_y - _y1) * sh / h) * srcStride; \
					placePixelMacro(); \
				} \
			} \
		dst_strideY += dstStride; \
		deformValues++; \
	}
#define DRAW_FLIPY(placePixelMacro) \
	for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
		i_y_rsin = -i_y * rsin; \
		i_y_rcos = i_y * rcos; \
		dstPxLine = dstPx + dst_strideY; \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		if (SoftwareRenderer::UseSpriteDeform) \
			for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; \
				dst_x++, i_x++) { \
				DEFORM_X; \
				src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
				src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
				if (src_x >= _x1 && src_y >= _y1 && src_x < _x2 && src_y < _y2) { \
					src_x = (src_x1 + (src_x - _x1) * sw / w); \
					src_strideY = \
						(src_y2 - (src_y - _y1) * sh / h) * srcStride; \
					placePixelMacro(); \
				} \
				dst_x -= *deformValues; \
			} \
		else \
			for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; \
				dst_x++, i_x++) { \
				src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
				src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
				if (src_x >= _x1 && src_y >= _y1 && src_x < _x2 && src_y < _y2) { \
					src_x = (src_x1 + (src_x - _x1) * sw / w); \
					src_strideY = \
						(src_y2 - (src_y - _y1) * sh / h) * srcStride; \
					placePixelMacro(); \
				} \
			} \
		dst_strideY += dstStride; \
		deformValues++; \
	}
#define DRAW_FLIPXY(placePixelMacro) \
	for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
		i_y_rsin = -i_y * rsin; \
		i_y_rcos = i_y * rcos; \
		dstPxLine = dstPx + dst_strideY; \
		if (Graphics::UsePaletteIndexLines) \
			index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]][0]; \
		if (SoftwareRenderer::UseSpriteDeform) \
			for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; \
				dst_x++, i_x++) { \
				DEFORM_X; \
				src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
				src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
				if (src_x >= _x1 && src_y >= _y1 && src_x < _x2 && src_y < _y2) { \
					src_x = (src_x2 - (src_x - _x1) * sw / w); \
					src_strideY = \
						(src_y2 - (src_y - _y1) * sh / h) * srcStride; \
					placePixelMacro(); \
				} \
				dst_x -= *deformValues; \
			} \
		else \
			for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; \
				dst_x++, i_x++) { \
				src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
				src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
				if (src_x >= _x1 && src_y >= _y1 && src_x < _x2 && src_y < _y2) { \
					src_x = (src_x2 - (src_x - _x1) * sw / w); \
					src_strideY = \
						(src_y2 - (src_y - _y1) * sh / h) * srcStride; \
					placePixelMacro(); \
				} \
			} \
		dst_strideY += dstStride; \
		deformValues++; \
	}

	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SoftwareRenderer::SetTintFunction(blendFlag);
	}

	Uint32 color;
	Uint32* index = nullptr;
	int i_y_rsin, i_y_rcos;
	int dst_strideY, src_strideY;
	int* multTableAt = &SoftwareRenderer::MultTable[opacity << 8];
	int* multSubTableAt = &SoftwareRenderer::MultSubTable[opacity << 8];
	Sint32* deformValues = &SoftwareRenderer::SpriteDeformBuffer[dst_y1];

	if (Graphics::UsePalettes && texture->Paletted) {
		if (!Graphics::UsePaletteIndexLines) {
			index = &Graphics::PaletteColors[paletteID][0];
		}

		switch (flipFlag) {
		case 0:
			dst_strideY = dst_y1 * dstStride;
			DRAW_NOFLIP(DRAW_PLACEPIXEL_PAL);
			break;
		case 1:
			dst_strideY = dst_y1 * dstStride;
			DRAW_FLIPX(DRAW_PLACEPIXEL_PAL);
			break;
		case 2:
			dst_strideY = dst_y1 * dstStride;
			DRAW_FLIPY(DRAW_PLACEPIXEL_PAL);
			break;
		case 3:
			dst_strideY = dst_y1 * dstStride;
			DRAW_FLIPXY(DRAW_PLACEPIXEL_PAL);
			break;
		}
	}
	else {
		switch (flipFlag) {
		case 0:
			dst_strideY = dst_y1 * dstStride;
			DRAW_NOFLIP(DRAW_PLACEPIXEL);
			break;
		case 1:
			dst_strideY = dst_y1 * dstStride;
			DRAW_FLIPX(DRAW_PLACEPIXEL);
			break;
		case 2:
			dst_strideY = dst_y1 * dstStride;
			DRAW_FLIPY(DRAW_PLACEPIXEL);
			break;
		case 3:
			dst_strideY = dst_y1 * dstStride;
			DRAW_FLIPXY(DRAW_PLACEPIXEL);
			break;
		}
	}

#undef DRAW_PLACEPIXEL
#undef DRAW_PLACEPIXEL_PAL
#undef DRAW_NOFLIP
#undef DRAW_FLIPX
#undef DRAW_FLIPY
#undef DRAW_FLIPXY
}

void SoftwareRenderer::DrawTexture(Texture* texture,
	float sx,
	float sy,
	float sw,
	float sh,
	float x,
	float y,
	float w,
	float h) {
	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	int textureWidth = texture->Width;
	int textureHeight = texture->Height;

	if (sw >= textureWidth - sx) {
		sw = textureWidth - sx;
	}
	if (sh >= textureHeight - sy) {
		sh = textureHeight - sy;
	}

	BlendState blendState = GetBlendState();
	if (w != textureWidth || h != textureHeight) {
		DrawSpriteImageTransformed(
			texture, x, y, sx, sy, w, h, sx, sy, sw, sh, 0, 0, 0, blendState);
	}
	else {
		DrawSpriteImage(texture, x, y, sw, sh, sx, sy, 0, 0, blendState);
	}
}
void SoftwareRenderer::DrawSprite(ISprite* sprite,
	int animation,
	int frame,
	int x,
	int y,
	bool flipX,
	bool flipY,
	float scaleW,
	float scaleH,
	float rotation,
	unsigned paletteID) {
	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) {
		return;
	}

	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	AnimFrame frameStr = sprite->Animations[animation].Frames[frame];
	Texture* texture = sprite->Spritesheets[frameStr.SheetNumber];

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	BlendState blendState = GetBlendState();
	int flipFlag = (int)flipX | ((int)flipY << 1);
	if (rotation != 0.0f || scaleW != 1.0f || scaleH != 1.0f) {
		int rot = (int)(rotation * TRIG_TABLE_HALF / M_PI) & TRIG_TABLE_MASK;
		DrawSpriteImageTransformed(texture,
			x,
			y,
			frameStr.OffsetX * scaleW,
			frameStr.OffsetY * scaleH,
			frameStr.Width * scaleW,
			frameStr.Height * scaleH,

			frameStr.X,
			frameStr.Y,
			frameStr.Width,
			frameStr.Height,
			flipFlag,
			rot,
			paletteID,
			blendState);
		return;
	}
	switch (flipFlag) {
	case 0:
		DrawSpriteImage(texture,
			x + frameStr.OffsetX,
			y + frameStr.OffsetY,
			frameStr.Width,
			frameStr.Height,
			frameStr.X,
			frameStr.Y,
			flipFlag,
			paletteID,
			blendState);
		break;
	case 1:
		DrawSpriteImage(texture,
			x - frameStr.OffsetX - frameStr.Width,
			y + frameStr.OffsetY,
			frameStr.Width,
			frameStr.Height,
			frameStr.X,
			frameStr.Y,
			flipFlag,
			paletteID,
			blendState);
		break;
	case 2:
		DrawSpriteImage(texture,
			x + frameStr.OffsetX,
			y - frameStr.OffsetY - frameStr.Height,
			frameStr.Width,
			frameStr.Height,
			frameStr.X,
			frameStr.Y,
			flipFlag,
			paletteID,
			blendState);
		break;
	case 3:
		DrawSpriteImage(texture,
			x - frameStr.OffsetX - frameStr.Width,
			y - frameStr.OffsetY - frameStr.Height,
			frameStr.Width,
			frameStr.Height,
			frameStr.X,
			frameStr.Y,
			flipFlag,
			paletteID,
			blendState);
		break;
	}
}
void SoftwareRenderer::DrawSpritePart(ISprite* sprite,
	int animation,
	int frame,
	int sx,
	int sy,
	int sw,
	int sh,
	int x,
	int y,
	bool flipX,
	bool flipY,
	float scaleW,
	float scaleH,
	float rotation,
	unsigned paletteID) {
	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) {
		return;
	}

	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	AnimFrame frameStr = sprite->Animations[animation].Frames[frame];
	Texture* texture = sprite->Spritesheets[frameStr.SheetNumber];

	int cx = (int)std::floor(currentView->X);
	int cy = (int)std::floor(currentView->Y);

	Matrix4x4* out = Graphics::ModelViewMatrix;
	x += out->Values[12];
	y += out->Values[13];
	x -= cx;
	y -= cy;

	if (sw >= frameStr.Width - sx) {
		sw = frameStr.Width - sx;
	}
	if (sh >= frameStr.Height - sy) {
		sh = frameStr.Height - sy;
	}

	BlendState blendState = GetBlendState();
	int flipFlag = (int)flipX | ((int)flipY << 1);
	if (rotation != 0.0f || scaleW != 1.0f || scaleH != 1.0f) {
		int rot = (int)(rotation * TRIG_TABLE_HALF / M_PI) & TRIG_TABLE_MASK;
		DrawSpriteImageTransformed(texture,
			x,
			y,
			(frameStr.OffsetX + sx) * scaleW,
			(frameStr.OffsetY + sy) * scaleH,
			sw * scaleW,
			sh * scaleH,

			frameStr.X + sx,
			frameStr.Y + sy,
			sw,
			sh,
			flipFlag,
			rot,
			paletteID,
			blendState);
		return;
	}
	switch (flipFlag) {
	case 0:
		DrawSpriteImage(texture,
			x + frameStr.OffsetX + sx,
			y + frameStr.OffsetY + sy,
			sw,
			sh,
			frameStr.X + sx,
			frameStr.Y + sy,
			flipFlag,
			paletteID,
			blendState);
		break;
	case 1:
		DrawSpriteImage(texture,
			x - frameStr.OffsetX - sw - sx,
			y + frameStr.OffsetY + sy,
			sw,
			sh,
			frameStr.X + sx,
			frameStr.Y + sy,
			flipFlag,
			paletteID,
			blendState);
		break;
	case 2:
		DrawSpriteImage(texture,
			x + frameStr.OffsetX + sx,
			y - frameStr.OffsetY - sh - sy,
			sw,
			sh,
			frameStr.X + sx,
			frameStr.Y + sy,
			flipFlag,
			paletteID,
			blendState);
		break;
	case 3:
		DrawSpriteImage(texture,
			x - frameStr.OffsetX - sw - sx,
			y - frameStr.OffsetY - sh - sy,
			sw,
			sh,
			frameStr.X + sx,
			frameStr.Y + sy,
			flipFlag,
			paletteID,
			blendState);
		break;
	}
}

// Default Tile Display Line setup
void SoftwareRenderer::DrawTile(int tile, int x, int y, bool flipX, bool flipY) {}
void SoftwareRenderer::DrawSceneLayer_InitTileScanLines(SceneLayer* layer, View* currentView) {
	switch (layer->DrawBehavior) {
	case DrawBehavior_PGZ1_BG:
	case DrawBehavior_HorizontalParallax: {
		int viewX = (int)currentView->X;
		int viewY = (int)currentView->Y;
		// int viewWidth = (int)currentView->Width;
		int viewHeight = (int)currentView->Height;
		int layerWidth = layer->Width * 16;
		int layerHeight = layer->Height * 16;
		int layerOffsetX = layer->OffsetX;
		int layerOffsetY = layer->OffsetY;

		// Set parallax positions
		ScrollingInfo* info = &layer->ScrollInfos[0];
		for (int i = 0; i < layer->ScrollInfoCount; i++) {
			info->Offset = Scene::Frame * info->ConstantParallax;
			info->Position =
				(info->Offset +
					((viewX + layerOffsetX) * info->RelativeParallax)) >>
				8;
			if (layer->Flags & SceneLayer::FLAGS_REPEAT_Y) {
				info->Position %= layerWidth;
				if (info->Position < 0) {
					info->Position += layerWidth;
				}
			}
			info++;
		}

		// Create scan lines
		Sint64 scrollOffset = Scene::Frame * layer->ConstantY;
		Sint64 scrollLine =
			(scrollOffset + ((viewY + layerOffsetY) * layer->RelativeY)) >> 8;
		scrollLine %= layerHeight;
		if (scrollLine < 0) {
			scrollLine += layerHeight;
		}

		int* deformValues;
		Uint8* parallaxIndex;
		TileScanLine* scanLine;
		const int maxDeformLineMask = (MAX_DEFORM_LINES >> 1) - 1;

		scanLine = &TileScanLineBuffer[0];
		parallaxIndex = &layer->ScrollIndexes[scrollLine];
		deformValues =
			&layer->DeformSetA[(scrollLine + layer->DeformOffsetA) & maxDeformLineMask];
		for (int i = 0; i < layer->DeformSplitLine; i++) {
			// Set scan line start positions
			info = &layer->ScrollInfos[*parallaxIndex];
			scanLine->SrcX = info->Position;
			if (info->CanDeform) {
				scanLine->SrcX += *deformValues;
			}
			scanLine->SrcX <<= 16;
			scanLine->SrcY = scrollLine << 16;

			scanLine->DeltaX = 0x10000;
			scanLine->DeltaY = 0x0000;

			// Iterate lines
			// NOTE: There is no protection from
			// over-reading deform indexes past 512 here.
			scanLine++;
			scrollLine++;
			deformValues++;

			// If we've reach the last line of the layer,
			// return to the first.
			if (scrollLine == layerHeight) {
				scrollLine = 0;
				parallaxIndex = &layer->ScrollIndexes[scrollLine];
			}
			else {
				parallaxIndex++;
			}
		}

		deformValues =
			&layer->DeformSetB[(scrollLine + layer->DeformOffsetB) & maxDeformLineMask];
		for (int i = layer->DeformSplitLine; i < viewHeight; i++) {
			// Set scan line start positions
			info = &layer->ScrollInfos[*parallaxIndex];
			scanLine->SrcX = info->Position;
			if (info->CanDeform) {
				scanLine->SrcX += *deformValues;
			}
			scanLine->SrcX <<= 16;
			scanLine->SrcY = scrollLine << 16;

			scanLine->DeltaX = 0x10000;
			scanLine->DeltaY = 0x0000;

			// Iterate lines
			// NOTE: There is no protection from
			// over-reading deform indexes past 512 here.
			scanLine++;
			scrollLine++;
			deformValues++;

			// If we've reach the last line of the layer,
			// return to the first.
			if (scrollLine == layerHeight) {
				scrollLine = 0;
				parallaxIndex = &layer->ScrollIndexes[scrollLine];
			}
			else {
				parallaxIndex++;
			}
		}
		break;
	}
	case DrawBehavior_VerticalParallax: {
		break;
	}
	case DrawBehavior_CustomTileScanLines: {
		Sint64 scrollOffset = Scene::Frame * layer->ConstantY;
		Sint64 scrollPositionX =
			((scrollOffset +
				 (((int)currentView->X + layer->OffsetX) * layer->RelativeY)) >>
				8);
		scrollPositionX %= layer->Width * 16;
		scrollPositionX <<= 16;
		Sint64 scrollPositionY =
			((scrollOffset +
				 (((int)currentView->Y + layer->OffsetY) * layer->RelativeY)) >>
				8);
		scrollPositionY %= layer->Height * 16;
		scrollPositionY <<= 16;

		TileScanLine* scanLine = &TileScanLineBuffer[0];
		for (int i = 0; i < currentView->Height; i++) {
			scanLine->SrcX = scrollPositionX;
			scanLine->SrcY = scrollPositionY;
			scanLine->DeltaX = 0x10000;
			scanLine->DeltaY = 0x0;

			scrollPositionY += 0x10000;
			scanLine++;
		}

		break;
	}
	}
}

void SoftwareRenderer::DrawSceneLayer_HorizontalParallax(SceneLayer* layer, View* currentView) {
	static vector<Uint32> srcStrides;
	static vector<Uint32*> tileSources;
	static vector<Uint8> isPalettedSources;
	static vector<unsigned> paletteIDs;
	srcStrides.reserve(Scene::TileSpriteInfos.size());
	tileSources.reserve(Scene::TileSpriteInfos.size());
	isPalettedSources.reserve(Scene::TileSpriteInfos.size());
	paletteIDs.reserve(Scene::TileSpriteInfos.size());

	int dst_x1 = 0;
	int dst_y1 = 0;
	int dst_x2 = (int)Graphics::CurrentRenderTarget->Width;
	int dst_y2 = (int)Graphics::CurrentRenderTarget->Height;

	Uint32 srcStride = 0;

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;
	Uint32* dstPxLine;

	int clip_x1, clip_y1, clip_x2, clip_y2;
	GetClipRegion(clip_x1, clip_y1, clip_x2, clip_y2);
	if (!CheckClipRegion(clip_x1, clip_y1, clip_x2, clip_y2)) {
		return;
	}

	if (dst_x1 < clip_x1) {
		dst_x1 = clip_x1;
	}
	if (dst_y1 < clip_y1) {
		dst_y1 = clip_y1;
	}
	if (dst_x2 > clip_x2) {
		dst_x2 = clip_x2;
	}
	if (dst_y2 > clip_y2) {
		dst_y2 = clip_y2;
	}

	if (dst_x2 < 0 || dst_y2 < 0 || dst_x1 >= dst_x2 || dst_y1 >= dst_y2) {
		return;
	}

	bool canCollide = (layer->Flags & SceneLayer::FLAGS_COLLIDEABLE);

	int layerWidthInBits = layer->WidthInBits;
	int layerWidthInPixels = layer->Width * Scene::TileWidth;
	int layerWidth = layer->Width;
	int sourceTileCellX, sourceTileCellY;
	int tileID;

	BlendState blendState = GetBlendState();

	if (!Graphics::TextureBlend) {
		blendState.Mode = BlendMode_NORMAL;
		blendState.Opacity = 0xFF;
	}

	if (!AlterBlendState(blendState)) {
		return;
	}

	int blendFlag = blendState.Mode;
	int opacity = blendState.Opacity;
	if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
		SetTintFunction(blendFlag);
	}

	int* multTableAt = &MultTable[opacity << 8];
	int* multSubTableAt = &MultSubTable[opacity << 8];

	Uint32* tile;
	Uint32* color;
	Uint32* index;
	int dst_strideY = dst_y1 * dstStride;

	int viewWidth = (int)currentView->Width;
	int maxTileDraw = ((int)currentView->Stride / Scene::TileWidth) - 1;

	for (size_t i = 0; i < Scene::TileSpriteInfos.size(); i++) {
		TileSpriteInfo& info = Scene::TileSpriteInfos[i];
		AnimFrame& frameStr =
			info.Sprite->Animations[info.AnimationIndex].Frames[info.FrameIndex];
		Texture* texture = info.Sprite->Spritesheets[frameStr.SheetNumber];
		srcStrides[i] = srcStride = texture->Width;
		tileSources[i] = (&((Uint32*)texture->Pixels)[frameStr.X + frameStr.Y * srcStride]);
		isPalettedSources[i] = Graphics::UsePalettes && texture->Paletted;
		paletteIDs[i] = Scene::Tilesets[info.TilesetID].PaletteID;
	}

	Uint32 DRAW_COLLISION = 0;
	int c_pixelsOfTileRemaining, tileFlipOffset;
	TileConfig* baseTileCfg = NULL;
	if (Scene::TileCfg.size()) {
		size_t collisionPlane = Scene::ShowTileCollisionFlag - 1;
		if (collisionPlane < Scene::TileCfg.size()) {
			baseTileCfg = Scene::TileCfg[collisionPlane];
		}
	}

	bool usePaletteIndexLines = Graphics::UsePaletteIndexLines && layer->UsePaletteIndexLines;

	PixelFunction pixelFunction = GetPixelFunction(blendFlag);

	int j;
	TileScanLine* tScanLine = &TileScanLineBuffer[dst_y1];
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++, tScanLine++, dst_strideY += dstStride) {
		tScanLine->SrcX >>= 16;
		tScanLine->SrcY >>= 16;
		dstPxLine = dstPx + dst_strideY;

		bool isInLayer = tScanLine->SrcX >= 0 && tScanLine->SrcX < layerWidthInPixels;
		if (!isInLayer && layer->Flags & SceneLayer::FLAGS_REPEAT_X) {
			if (tScanLine->SrcX < 0) {
				tScanLine->SrcX = -(tScanLine->SrcX % layerWidthInPixels);
			}
			else {
				tScanLine->SrcX %= layerWidthInPixels;
			}
			isInLayer = true;
		}

		int dst_x = dst_x1, c_dst_x = dst_x1;
		int pixelsOfTileRemaining;
		Sint64 srcX = tScanLine->SrcX, srcY = tScanLine->SrcY;

		// Draw leftmost tile in scanline
		int srcTX = srcX & 15;
		int srcTY = srcY & 15;
		sourceTileCellX = (srcX >> 4);
		sourceTileCellY = (srcY >> 4);
		c_pixelsOfTileRemaining = srcTX;
		pixelsOfTileRemaining = 16 - srcTX;
		tile = &layer->Tiles[sourceTileCellX + (sourceTileCellY << layerWidthInBits)];

		if (isInLayer && (*tile & TILE_IDENT_MASK) != Scene::EmptyTile) {
			tileID = *tile & TILE_IDENT_MASK;
			if (usePaletteIndexLines) {
				index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]]
								[0];
			}
			else {
				index = &Graphics::PaletteColors[paletteIDs[tileID]][0];
			}
			if (Scene::ShowTileCollisionFlag && baseTileCfg) {
				c_dst_x = dst_x;
				if (Scene::ShowTileCollisionFlag == 1) {
					DRAW_COLLISION = (*tile & TILE_COLLA_MASK) >> 28;
				}
				else if (Scene::ShowTileCollisionFlag == 2) {
					DRAW_COLLISION = (*tile & TILE_COLLB_MASK) >> 26;
				}

				switch (DRAW_COLLISION) {
				case 1:
					DRAW_COLLISION = 0xFFFFFF00U;
					break;
				case 2:
					DRAW_COLLISION = 0xFFFF0000U;
					break;
				case 3:
					DRAW_COLLISION = 0xFFFFFFFFU;
					break;
				}
			}

			// If y-flipped
			if ((*tile & TILE_FLIPY_MASK)) {
				srcTY ^= 15;
			}
			// If x-flipped
			if ((*tile & TILE_FLIPX_MASK)) {
				srcTX ^= 15;
				color = &tileSources[tileID][srcTX + srcTY * srcStrides[tileID]];
				if (isPalettedSources[tileID]) {
					while (pixelsOfTileRemaining) {
						if (*color && (index[*color] & 0xFF000000U)) {
							pixelFunction(&index[*color],
								&dstPxLine[dst_x],
								blendState,
								multTableAt,
								multSubTableAt);
						}
						pixelsOfTileRemaining--;
						dst_x++;
						color--;
					}
				}
				else {
					while (pixelsOfTileRemaining) {
						if (*color & 0xFF000000U) {
							pixelFunction(color,
								&dstPxLine[dst_x],
								blendState,
								multTableAt,
								multSubTableAt);
						}
						pixelsOfTileRemaining--;
						dst_x++;
						color--;
					}
				}
			}
			// Otherwise
			else {
				color = &tileSources[tileID][srcTX + srcTY * srcStrides[tileID]];
				if (isPalettedSources[tileID]) {
					while (pixelsOfTileRemaining) {
						if (*color && (index[*color] & 0xFF000000U)) {
							pixelFunction(&index[*color],
								&dstPxLine[dst_x],
								blendState,
								multTableAt,
								multSubTableAt);
						}
						pixelsOfTileRemaining--;
						dst_x++;
						color++;
					}
				}
				else {
					while (pixelsOfTileRemaining) {
						if (*color & 0xFF000000U) {
							pixelFunction(color,
								&dstPxLine[dst_x],
								blendState,
								multTableAt,
								multSubTableAt);
						}
						pixelsOfTileRemaining--;
						dst_x++;
						color++;
					}
				}
			}

			if (canCollide && DRAW_COLLISION) {
				tileFlipOffset = (((!!(*tile & TILE_FLIPY_MASK)) << 1) |
							 (!!(*tile & TILE_FLIPX_MASK))) *
					Scene::TileCount;

				bool flipY = !!(*tile & TILE_FLIPY_MASK);
				bool isCeiling = !!baseTileCfg[tileID].IsCeiling;
				TileConfig* tile = (&baseTileCfg[tileID] + tileFlipOffset);
				for (int gg = c_pixelsOfTileRemaining; gg < 16; gg++) {
					if ((flipY == isCeiling &&
						    (srcY & 15) >= tile->CollisionTop[gg] &&
						    tile->CollisionTop[gg] < 0xF0) ||
						(flipY != isCeiling &&
							(srcY & 15) <= tile->CollisionBottom[gg] &&
							tile->CollisionBottom[gg] < 0xF0)) {
						PixelNoFiltSetOpaque(&DRAW_COLLISION,
							&dstPxLine[c_dst_x],
							CurrentBlendState,
							NULL,
							NULL);
					}
					c_dst_x++;
				}
			}
		}
		else {
			dst_x += pixelsOfTileRemaining;
		}

		// Draw scanline tiles in batches of 16 pixels
		srcTY = srcY & 15;
		for (j = maxTileDraw; j; j--, dst_x += 16) {
			sourceTileCellX++;
			tile++;
			if (sourceTileCellX < 0) {
				continue;
			}
			else if (sourceTileCellX >= layerWidth) {
				if (layer->Flags & SceneLayer::FLAGS_REPEAT_X) {
					sourceTileCellX -= layerWidth;
					tile -= layerWidth;
				}
				else {
					break;
				}
			}

			if (Scene::ShowTileCollisionFlag && baseTileCfg) {
				c_dst_x = dst_x;
				if (Scene::ShowTileCollisionFlag == 1) {
					DRAW_COLLISION = (*tile & TILE_COLLA_MASK) >> 28;
				}
				else if (Scene::ShowTileCollisionFlag == 2) {
					DRAW_COLLISION = (*tile & TILE_COLLB_MASK) >> 26;
				}

				switch (DRAW_COLLISION) {
				case 1:
					DRAW_COLLISION = 0xFFFFFF00U;
					break;
				case 2:
					DRAW_COLLISION = 0xFFFF0000U;
					break;
				case 3:
					DRAW_COLLISION = 0xFFFFFFFFU;
					break;
				}
			}

			int srcTYb = srcTY;
			tileID = *tile & TILE_IDENT_MASK;
			if (tileID != Scene::EmptyTile) {
				if (usePaletteIndexLines) {
					index = &Graphics::PaletteColors
							[Graphics::PaletteIndexLines[dst_y]][0];
				}
				else {
					index = &Graphics::PaletteColors[paletteIDs[tileID]][0];
				}
				// If y-flipped
				if ((*tile & TILE_FLIPY_MASK)) {
					srcTYb ^= 15;
				}
				// If x-flipped
				if ((*tile & TILE_FLIPX_MASK)) {
					color = &tileSources[tileID][srcTYb * srcStrides[tileID]];
					if (isPalettedSources[tileID]) {
#define UNLOOPED(n, k) \
	if (color[n] && (index[color[n]] & 0xFF000000U)) { \
		pixelFunction(&index[color[n]], \
			&dstPxLine[dst_x + k], \
			blendState, \
			multTableAt, \
			multSubTableAt); \
	}
						UNLOOPED(0, 15);
						UNLOOPED(1, 14);
						UNLOOPED(2, 13);
						UNLOOPED(3, 12);
						UNLOOPED(4, 11);
						UNLOOPED(5, 10);
						UNLOOPED(6, 9);
						UNLOOPED(7, 8);
						UNLOOPED(8, 7);
						UNLOOPED(9, 6);
						UNLOOPED(10, 5);
						UNLOOPED(11, 4);
						UNLOOPED(12, 3);
						UNLOOPED(13, 2);
						UNLOOPED(14, 1);
						UNLOOPED(15, 0);
#undef UNLOOPED
					}
					else {
#define UNLOOPED(n, k) \
	if (color[n] & 0xFF000000U) { \
		pixelFunction(&color[n], \
			&dstPxLine[dst_x + k], \
			blendState, \
			multTableAt, \
			multSubTableAt); \
	}
						UNLOOPED(0, 15);
						UNLOOPED(1, 14);
						UNLOOPED(2, 13);
						UNLOOPED(3, 12);
						UNLOOPED(4, 11);
						UNLOOPED(5, 10);
						UNLOOPED(6, 9);
						UNLOOPED(7, 8);
						UNLOOPED(8, 7);
						UNLOOPED(9, 6);
						UNLOOPED(10, 5);
						UNLOOPED(11, 4);
						UNLOOPED(12, 3);
						UNLOOPED(13, 2);
						UNLOOPED(14, 1);
						UNLOOPED(15, 0);
#undef UNLOOPED
					}
				}
				// Otherwise
				else {
					color = &tileSources[tileID][srcTYb * srcStrides[tileID]];
					if (isPalettedSources[tileID]) {
#define UNLOOPED(n, k) \
	if (color[n] && (index[color[n]] & 0xFF000000U)) { \
		pixelFunction(&index[color[n]], \
			&dstPxLine[dst_x + k], \
			blendState, \
			multTableAt, \
			multSubTableAt); \
	}
						UNLOOPED(0, 0);
						UNLOOPED(1, 1);
						UNLOOPED(2, 2);
						UNLOOPED(3, 3);
						UNLOOPED(4, 4);
						UNLOOPED(5, 5);
						UNLOOPED(6, 6);
						UNLOOPED(7, 7);
						UNLOOPED(8, 8);
						UNLOOPED(9, 9);
						UNLOOPED(10, 10);
						UNLOOPED(11, 11);
						UNLOOPED(12, 12);
						UNLOOPED(13, 13);
						UNLOOPED(14, 14);
						UNLOOPED(15, 15);
#undef UNLOOPED
					}
					else {
#define UNLOOPED(n, k) \
	if (color[n] & 0xFF000000U) { \
		pixelFunction(&color[n], \
			&dstPxLine[dst_x + k], \
			blendState, \
			multTableAt, \
			multSubTableAt); \
	}
						UNLOOPED(0, 0);
						UNLOOPED(1, 1);
						UNLOOPED(2, 2);
						UNLOOPED(3, 3);
						UNLOOPED(4, 4);
						UNLOOPED(5, 5);
						UNLOOPED(6, 6);
						UNLOOPED(7, 7);
						UNLOOPED(8, 8);
						UNLOOPED(9, 9);
						UNLOOPED(10, 10);
						UNLOOPED(11, 11);
						UNLOOPED(12, 12);
						UNLOOPED(13, 13);
						UNLOOPED(14, 14);
						UNLOOPED(15, 15);
#undef UNLOOPED
					}
				}

				if (canCollide && DRAW_COLLISION) {
					tileFlipOffset = (((!!(*tile & TILE_FLIPY_MASK)) << 1) |
								 (!!(*tile & TILE_FLIPX_MASK))) *
						Scene::TileCount;

					bool flipY = !!(*tile & TILE_FLIPY_MASK);
					bool isCeiling = !!baseTileCfg[tileID].IsCeiling;
					TileConfig* tile = (&baseTileCfg[tileID] + tileFlipOffset);
					for (int gg = 0; gg < 16; gg++) {
						if ((flipY == isCeiling &&
							    (srcY & 15) >= tile->CollisionTop[gg] &&
							    tile->CollisionTop[gg] < 0xF0) ||
							(flipY != isCeiling &&
								(srcY & 15) <=
									tile->CollisionBottom[gg] &&
								tile->CollisionBottom[gg] < 0xF0)) {
							PixelNoFiltSetOpaque(&DRAW_COLLISION,
								&dstPxLine[c_dst_x],
								CurrentBlendState,
								NULL,
								NULL);
						}
						c_dst_x++;
					}
				}
			}
		}

		if (dst_x >= viewWidth) {
			continue;
		}

		// Draw rightmost tile in scanline
		srcX += (maxTileDraw + 1) * Scene::TileWidth;
		if (srcX < 0 || srcX >= layerWidthInPixels) {
			if ((layer->Flags & SceneLayer::FLAGS_REPEAT_X) == 0) {
				continue;
			}

			if (srcX < 0) {
				srcX += layerWidthInPixels;
			}
			else if (srcX >= layerWidthInPixels) {
				srcX -= layerWidthInPixels;
			}
		}

		srcTX = 0;
		sourceTileCellX = (srcX >> 4);
		pixelsOfTileRemaining = std::min(viewWidth - dst_x, 16);
		c_pixelsOfTileRemaining = pixelsOfTileRemaining;
		tile = &layer->Tiles[sourceTileCellX + (sourceTileCellY << layerWidthInBits)];

		if ((*tile & TILE_IDENT_MASK) != Scene::EmptyTile) {
			tileID = *tile & TILE_IDENT_MASK;
			if (usePaletteIndexLines) {
				index = &Graphics::PaletteColors[Graphics::PaletteIndexLines[dst_y]]
								[0];
			}
			else {
				index = &Graphics::PaletteColors[paletteIDs[tileID]][0];
			}
			if (Scene::ShowTileCollisionFlag && baseTileCfg) {
				c_dst_x = dst_x;
				if (Scene::ShowTileCollisionFlag == 1) {
					DRAW_COLLISION = (*tile & TILE_COLLA_MASK) >> 28;
				}
				else if (Scene::ShowTileCollisionFlag == 2) {
					DRAW_COLLISION = (*tile & TILE_COLLB_MASK) >> 26;
				}

				switch (DRAW_COLLISION) {
				case 1:
					DRAW_COLLISION = 0xFFFFFF00U;
					break;
				case 2:
					DRAW_COLLISION = 0xFFFF0000U;
					break;
				case 3:
					DRAW_COLLISION = 0xFFFFFFFFU;
					break;
				}
			}

			// If y-flipped
			if ((*tile & TILE_FLIPY_MASK)) {
				srcTY ^= 15;
			}
			// If x-flipped
			if ((*tile & TILE_FLIPX_MASK)) {
				srcTX ^= 15;
				color = &tileSources[tileID][srcTX + srcTY * srcStrides[tileID]];
				if (isPalettedSources[tileID]) {
					while (pixelsOfTileRemaining) {
						if (*color && (index[*color] & 0xFF000000U)) {
							pixelFunction(&index[*color],
								&dstPxLine[dst_x],
								blendState,
								multTableAt,
								multSubTableAt);
						}
						pixelsOfTileRemaining--;
						dst_x++;
						color--;
					}
				}
				else {
					while (pixelsOfTileRemaining) {
						if (*color & 0xFF000000U) {
							pixelFunction(color,
								&dstPxLine[dst_x],
								blendState,
								multTableAt,
								multSubTableAt);
						}
						pixelsOfTileRemaining--;
						dst_x++;
						color--;
					}
				}
			}
			// Otherwise
			else {
				color = &tileSources[tileID][srcTX + srcTY * srcStrides[tileID]];
				if (isPalettedSources[tileID]) {
					while (pixelsOfTileRemaining) {
						if (*color && (index[*color] & 0xFF000000U)) {
							pixelFunction(&index[*color],
								&dstPxLine[dst_x],
								blendState,
								multTableAt,
								multSubTableAt);
						}
						pixelsOfTileRemaining--;
						dst_x++;
						color++;
					}
				}
				else {
					while (pixelsOfTileRemaining) {
						if (*color & 0xFF000000U) {
							pixelFunction(color,
								&dstPxLine[dst_x],
								blendState,
								multTableAt,
								multSubTableAt);
						}
						pixelsOfTileRemaining--;
						dst_x++;
						color++;
					}
				}
			}

			if (canCollide && DRAW_COLLISION) {
				tileFlipOffset = (((!!(*tile & TILE_FLIPY_MASK)) << 1) |
							 (!!(*tile & TILE_FLIPX_MASK))) *
					Scene::TileCount;

				bool flipY = !!(*tile & TILE_FLIPY_MASK);
				bool isCeiling = !!baseTileCfg[tileID].IsCeiling;
				TileConfig* tile = (&baseTileCfg[tileID] + tileFlipOffset);
				for (int gg = c_pixelsOfTileRemaining; gg < 16; gg++) {
					if ((flipY == isCeiling &&
						    (srcY & 15) >= tile->CollisionTop[gg] &&
						    tile->CollisionTop[gg] < 0xF0) ||
						(flipY != isCeiling &&
							(srcY & 15) <= tile->CollisionBottom[gg] &&
							tile->CollisionBottom[gg] < 0xF0)) {
						PixelNoFiltSetOpaque(&DRAW_COLLISION,
							&dstPxLine[c_dst_x],
							CurrentBlendState,
							NULL,
							NULL);
					}
					c_dst_x++;
				}
			}
		}
	}
}
void SoftwareRenderer::DrawSceneLayer_VerticalParallax(SceneLayer* layer, View* currentView) {}
void SoftwareRenderer::DrawSceneLayer_CustomTileScanLines(SceneLayer* layer, View* currentView) {
	static vector<Uint32> srcStrides;
	static vector<Uint32*> tileSources;
	static vector<Uint8> isPalettedSources;
	static vector<unsigned> paletteIDs;
	srcStrides.reserve(Scene::TileSpriteInfos.size());
	tileSources.reserve(Scene::TileSpriteInfos.size());
	isPalettedSources.reserve(Scene::TileSpriteInfos.size());
	paletteIDs.reserve(Scene::TileSpriteInfos.size());

	int dst_x1 = 0;
	int dst_y1 = 0;
	int dst_x2 = (int)Graphics::CurrentRenderTarget->Width;
	int dst_y2 = (int)Graphics::CurrentRenderTarget->Height;

	// Uint32* srcPx = NULL;
	Uint32 srcStride = 0;
	// Uint32* srcPxLine;

	Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
	Uint32 dstStride = Graphics::CurrentRenderTarget->Width;
	Uint32* dstPxLine;

	int clip_x1, clip_y1, clip_x2, clip_y2;
	GetClipRegion(clip_x1, clip_y1, clip_x2, clip_y2);
	if (!CheckClipRegion(clip_x1, clip_y1, clip_x2, clip_y2)) {
		return;
	}

	if (dst_x1 < clip_x1) {
		dst_x1 = clip_x1;
	}
	if (dst_y1 < clip_y1) {
		dst_y1 = clip_y1;
	}
	if (dst_x2 > clip_x2) {
		dst_x2 = clip_x2;
	}
	if (dst_y2 > clip_y2) {
		dst_y2 = clip_y2;
	}

	if (dst_x2 < 0 || dst_y2 < 0 || dst_x1 >= dst_x2 || dst_y1 >= dst_y2) {
		return;
	}

	int layerWidthInBits = layer->WidthInBits;
	int layerWidthTileMask = layer->WidthMask;
	int layerHeightTileMask = layer->HeightMask;
	int tile, sourceTileCellX, sourceTileCellY;

	Uint32 color;
	Uint32* index;
	int dst_strideY = dst_y1 * dstStride;

	for (size_t i = 0; i < Scene::TileSpriteInfos.size(); i++) {
		TileSpriteInfo& info = Scene::TileSpriteInfos[i];
		AnimFrame& frameStr =
			info.Sprite->Animations[info.AnimationIndex].Frames[info.FrameIndex];
		Texture* texture = info.Sprite->Spritesheets[frameStr.SheetNumber];
		srcStrides[i] = srcStride = texture->Width;
		tileSources[i] = (&((Uint32*)texture->Pixels)[frameStr.X + frameStr.Y * srcStride]);
		isPalettedSources[i] = Graphics::UsePalettes && texture->Paletted;
		paletteIDs[i] = Scene::Tilesets[info.TilesetID].PaletteID;
	}

	bool usePaletteIndexLines = Graphics::UsePaletteIndexLines && layer->UsePaletteIndexLines;

	TileScanLine* scanLine = &TileScanLineBuffer[dst_y1];
	for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
		dstPxLine = dstPx + dst_strideY;

		Sint64 srcX = scanLine->SrcX, srcY = scanLine->SrcY, srcDX = scanLine->DeltaX,
		       srcDY = scanLine->DeltaY;

		Uint32 maxHorzCells = scanLine->MaxHorzCells;
		Uint32 maxVertCells = scanLine->MaxVertCells;

		PixelFunction linePixelFunction = NULL;

		BlendState blendState = GetBlendState();
		if (Graphics::TextureBlend) {
			blendState.Opacity -= 0xFF - scanLine->Opacity;
			if (blendState.Opacity < 0) {
				blendState.Opacity = 0;
			}
		}
		else {
			blendState.Mode = BlendFlag_OPAQUE;
			blendState.Opacity = 0xFF;
		}

		int* multTableAt;
		int* multSubTableAt;
		int blendFlag;

		if (!AlterBlendState(blendState)) {
			goto scanlineDone;
		}

		blendFlag = blendState.Mode;
		multTableAt = &MultTable[blendState.Opacity << 8];
		multSubTableAt = &MultSubTable[blendState.Opacity << 8];

		// TODO: Set CurrentPixelFunction instead whenever this
		// supports the stencil.
		if (blendFlag & (BlendFlag_TINT_BIT | BlendFlag_FILTER_BIT)) {
			linePixelFunction = PixelTintFunctions[blendFlag & BlendFlag_MODE_MASK];
			SetTintFunction(blendFlag);
		}
		else {
			linePixelFunction = PixelNoFiltFunctions[blendFlag & BlendFlag_MODE_MASK];
		}

		for (int dst_x = dst_x1; dst_x < dst_x2; dst_x++) {
			int srcTX = srcX >> 16;
			int srcTY = srcY >> 16;

			sourceTileCellX = (srcX >> 20) & layerWidthTileMask;
			sourceTileCellY = (srcY >> 20) & layerHeightTileMask;

			if (maxHorzCells != 0) {
				sourceTileCellX %= maxHorzCells;
			}
			if (maxVertCells != 0) {
				sourceTileCellY %= maxVertCells;
			}

			tile = layer->Tiles[sourceTileCellX +
				(sourceTileCellY << layerWidthInBits)];

			if ((tile & TILE_IDENT_MASK) != Scene::EmptyTile) {
				int tileID = tile & TILE_IDENT_MASK;

				if (usePaletteIndexLines) {
					index = &Graphics::PaletteColors
							[Graphics::PaletteIndexLines[dst_y]][0];
				}
				else {
					index = &Graphics::PaletteColors[paletteIDs[tileID]][0];
				}

				// If y-flipped
				if (tile & TILE_FLIPY_MASK) {
					srcTY ^= 15;
				}
				// If x-flipped
				if (tile & TILE_FLIPX_MASK) {
					srcTX ^= 15;
				}

				color = tileSources[tileID][(srcTX & 15) +
					(srcTY & 15) * srcStrides[tileID]];
				if (isPalettedSources[tileID]) {
					if (color && (index[color] & 0xFF000000U)) {
						linePixelFunction(&index[color],
							&dstPxLine[dst_x],
							blendState,
							multTableAt,
							multSubTableAt);
					}
				}
				else {
					if (color & 0xFF000000U) {
						linePixelFunction(&color,
							&dstPxLine[dst_x],
							blendState,
							multTableAt,
							multSubTableAt);
					}
				}
			}
			srcX += srcDX;
			srcY += srcDY;
		}

	scanlineDone:
		scanLine++;
		dst_strideY += dstStride;
	}
}
void SoftwareRenderer::DrawSceneLayer(SceneLayer* layer,
	View* currentView,
	int layerIndex,
	bool useCustomFunction) {
	if (layer->UsingCustomRenderFunction && useCustomFunction) {
		Graphics::RunCustomSceneLayerFunction(&layer->CustomRenderFunction, layerIndex);
		return;
	}

	if (layer->UsingCustomScanlineFunction &&
		layer->DrawBehavior == DrawBehavior_CustomTileScanLines) {
		Graphics::RunCustomSceneLayerFunction(&layer->CustomScanlineFunction, layerIndex);
	}
	else {
		SoftwareRenderer::DrawSceneLayer_InitTileScanLines(layer, currentView);
	}

	switch (layer->DrawBehavior) {
	case DrawBehavior_PGZ1_BG:
	case DrawBehavior_HorizontalParallax:
		SoftwareRenderer::DrawSceneLayer_HorizontalParallax(layer, currentView);
		break;
	case DrawBehavior_VerticalParallax:
		SoftwareRenderer::DrawSceneLayer_VerticalParallax(layer, currentView);
		break;
	case DrawBehavior_CustomTileScanLines:
		SoftwareRenderer::DrawSceneLayer_CustomTileScanLines(layer, currentView);
		break;
	}
}

void SoftwareRenderer::MakeFrameBufferID(ISprite* sprite) {
	sprite->ID = 0;
}
