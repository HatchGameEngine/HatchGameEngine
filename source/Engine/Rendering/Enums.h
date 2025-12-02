#ifndef ENGINE_RENDERING_ENUMS
#define ENGINE_RENDERING_ENUMS

#include "Engine/Includes/Standard.h"

enum { PixelFormat_ARGB8888, PixelFormat_ABGR8888 };

enum {
	// The texture is created with the given pixel data and never updated again, or it updates
	// rarely. A texture with access TextureAccess_STATIC can become TextureAccess_STREAMING and
	// the other way around.
	TextureAccess_STATIC,

	// The texture is expected to be updated often.
	TextureAccess_STREAMING,

	// The texture can be used as a render target.
	// If a texture is created with TextureAccess_RENDERTARGET, it cannot be reinitialized.
	// Which is to say, if you want to change its size or type, a new texture must be created.
	// Render targets must not be indexed, also.
	TextureAccess_RENDERTARGET
};

enum {
	TextureFormat_ARGB8888,
	TextureFormat_ABGR8888,
	TextureFormat_RGBA8888,
	TextureFormat_INDEXED,
	TextureFormat_YV12,
	TextureFormat_IYUV,
	TextureFormat_YUY2,
	TextureFormat_UYVY,
	TextureFormat_YVYU,
	TextureFormat_NV12,
	TextureFormat_NV21
};

#define TEXTUREFORMAT_IS_RGBA(fmt) \
	((fmt) == TextureFormat_ARGB8888 || (fmt) == TextureFormat_ABGR8888 || \
		(fmt) == TextureFormat_RGBA8888)

#define TEXTUREFORMAT_IS_YUV(fmt) ((fmt) >= TextureFormat_YV12 && (fmt) <= TextureFormat_NV21)

enum {
	TextureFilter_NEAREST,
	TextureFilter_LINEAR,
	TextureFilter_NEAREST_MIPMAP_NEAREST,
	TextureFilter_LINEAR_MIPMAP_NEAREST,
	TextureFilter_NEAREST_MIPMAP_LINEAR,
	TextureFilter_LINEAR_MIPMAP_LINEAR
};

enum {
	BlendMode_NORMAL = 0,
	BlendMode_ADD = 1,
	BlendMode_MAX = 2,
	BlendMode_SUBTRACT = 3,
	BlendMode_MATCH_EQUAL = 4,
	BlendMode_MATCH_NOT_EQUAL = 5,
};

enum {
	BlendFactor_ZERO = 0,
	BlendFactor_ONE = 1,
	BlendFactor_SRC_COLOR = 2,
	BlendFactor_INV_SRC_COLOR = 3,
	BlendFactor_SRC_ALPHA = 4,
	BlendFactor_INV_SRC_ALPHA = 5,
	BlendFactor_DST_COLOR = 6,
	BlendFactor_INV_DST_COLOR = 7,
	BlendFactor_DST_ALPHA = 8,
	BlendFactor_INV_DST_ALPHA = 9,
};

enum { TintMode_SRC_NORMAL, TintMode_DST_NORMAL, TintMode_SRC_BLEND, TintMode_DST_BLEND };

enum { Filter_NONE, Filter_BLACK_AND_WHITE, Filter_INVERT };

enum {
	StencilTest_Never,
	StencilTest_Always,
	StencilTest_Equal,
	StencilTest_NotEqual,
	StencilTest_Less,
	StencilTest_Greater,
	StencilTest_LEqual,
	StencilTest_GEqual
};

enum {
	StencilOp_Keep,
	StencilOp_Zero,
	StencilOp_Incr,
	StencilOp_Decr,
	StencilOp_Invert,
	StencilOp_Replace,
	StencilOp_IncrWrap,
	StencilOp_DecrWrap
};

enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

enum {
	DrawBehavior_HorizontalParallax = 0,
	DrawBehavior_VerticalParallax = 1,
	DrawBehavior_CustomTileScanLines = 2
};

enum {
	DrawMode_POLYGONS = 0x0, // 0b0000
	DrawMode_LINES = 0x1, // 0b0001
	DrawMode_POINTS = 0x2, // 0b0010
	DrawMode_PrimitiveMask = 0x3, // 0b0011

	DrawMode_FLAT_LIGHTING = 0x4, // 0b0100
	DrawMode_SMOOTH_LIGHTING = 0x8, // 0b1000
	DrawMode_LightingMask = 0xC, // 0b1100

	DrawMode_LINES_FLAT = DrawMode_LINES | DrawMode_FLAT_LIGHTING,
	DrawMode_LINES_SMOOTH = DrawMode_LINES | DrawMode_SMOOTH_LIGHTING,

	DrawMode_POLYGONS_FLAT = DrawMode_POLYGONS | DrawMode_FLAT_LIGHTING,
	DrawMode_POLYGONS_SMOOTH = DrawMode_POLYGONS | DrawMode_SMOOTH_LIGHTING,

	DrawMode_FillTypeMask = 0xF, // 0b1111

	DrawMode_TEXTURED = 1 << 4,
	DrawMode_AFFINE = 1 << 5,
	DrawMode_DEPTH_TEST = 1 << 6,
	DrawMode_FOG = 1 << 7,
	DrawMode_ORTHOGRAPHIC = 1 << 8,
	DrawMode_FlagsMask = ~0xF
};

enum { TEXTDRAW_ELLIPSIS = 1 << 0 };

struct TileScanLine {
	Sint64 SrcX;
	Sint64 SrcY;
	Sint64 DeltaX;
	Sint64 DeltaY;
	Uint8 Opacity;
	Uint32 MaxHorzCells;
	Uint32 MaxVertCells;
};
struct Viewport {
	float X;
	float Y;
	float Width;
	float Height;
};
struct ClipArea {
	bool Enabled;
	float X;
	float Y;
	float Width;
	float Height;
};
struct Point {
	float X;
	float Y;
	float Z;
};
// TODO: Graphics should have a pointer to a GraphicsState.
struct GraphicsState {
	void* CurrentShader;
	Viewport CurrentViewport;
	ClipArea CurrentClip;
	float BlendColors[4];
	float TintColors[4];
	int BlendMode;
	int TintMode;
	bool TextureBlend;
	bool UseTinting;
	bool UseDepthTesting;
	bool UsePalettes;
	bool UsePaletteIndexLines;
};
struct TintState {
	bool Enabled;
	Uint32 Color;
	Uint16 Amount;
	Uint8 Mode;
};
struct BlendState {
	int Opacity;
	int Mode;
	TintState Tint;
	int FilterMode;
};

typedef void (*PixelFunction)(Uint32*, Uint32*, BlendState&, int*, int*);
typedef Uint32 (*TintFunction)(Uint32*, Uint32*, Uint32, Uint32);
typedef bool (*StencilTestFunction)(Uint8*, Uint8, Uint8);
typedef void (*StencilOpFunction)(Uint8*, Uint8);

struct TextDrawParams {
	float FontSize = 0.0f;
	float Ascent = 0.0f;
	float Descent = 0.0f;
	float Leading = 0.0f;
	int MaxWidth = 0;
	int MaxLines = 0;
	Uint8 Flags = 0;
};

struct LegacyTextDrawParams {
	float Align = 0.0f;
	float Baseline = 0.0f;
	float Ascent = 0.0f;
	float Advance = 0.0f;
	int MaxWidth = 0;
	int MaxLines = 0;
	Uint8 Flags = 0;
};

#endif /* ENGINE_RENDERING_ENUMS */
