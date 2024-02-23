#ifndef ENGINE_RENDERING_ENUMS
#define ENGINE_RENDERING_ENUMS

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

enum {
    TintMode_SRC_NORMAL,
    TintMode_DST_NORMAL,
    TintMode_SRC_BLEND,
    TintMode_DST_BLEND
};

enum {
    Filter_NONE,
    Filter_BLACK_AND_WHITE,
    Filter_INVERT
};

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

enum {
    DrawBehavior_HorizontalParallax = 0,
    DrawBehavior_VerticalParallax = 1,
    DrawBehavior_CustomTileScanLines = 2,
    DrawBehavior_PGZ1_BG = 3,
};

enum {
    DrawMode_POLYGONS        = 0x0, // 0b0000
    DrawMode_LINES           = 0x1, // 0b0001
    DrawMode_POINTS          = 0x2, // 0b0010
    DrawMode_PrimitiveMask   = 0x3, // 0b0011

    DrawMode_FLAT_LIGHTING   = 0x4, // 0b0100
    DrawMode_SMOOTH_LIGHTING = 0x8, // 0b1000
    DrawMode_LightingMask    = 0xC, // 0b1100

    DrawMode_LINES_FLAT      = DrawMode_LINES | DrawMode_FLAT_LIGHTING,
    DrawMode_LINES_SMOOTH    = DrawMode_LINES | DrawMode_SMOOTH_LIGHTING,

    DrawMode_POLYGONS_FLAT   = DrawMode_POLYGONS | DrawMode_FLAT_LIGHTING,
    DrawMode_POLYGONS_SMOOTH = DrawMode_POLYGONS | DrawMode_SMOOTH_LIGHTING,

    DrawMode_FillTypeMask    = 0xF, // 0b1111

    DrawMode_TEXTURED        = 1<<4,
    DrawMode_AFFINE          = 1<<5,
    DrawMode_DEPTH_TEST      = 1<<6,
    DrawMode_FOG             = 1<<7,
    DrawMode_ORTHOGRAPHIC    = 1<<8,
    DrawMode_FlagsMask       = ~0xF
};

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
struct GraphicsState {
    Viewport           CurrentViewport;
    ClipArea           CurrentClip;
    float              BlendColors[4];
    float              TintColors[4];
    int                BlendMode;
    int                TintMode;
    bool               TextureBlend;
    bool               UseTinting;
    bool               UseDepthTesting;
    bool               UsePalettes;
    bool               UsePaletteIndexLines;
};
struct TintState {
    bool   Enabled;
    Uint32 Color;
    Uint16 Amount;
    Uint8  Mode;
};
struct BlendState {
    int Opacity;
    int Mode;
    TintState Tint;
    int *FilterTable;
};

typedef void (*PixelFunction)(Uint32*, Uint32*, BlendState&, int*, int*);
typedef Uint32 (*TintFunction)(Uint32*, Uint32*, Uint32, Uint32);
typedef bool (*StencilTestFunction)(Uint8*, Uint8, Uint8);
typedef void (*StencilOpFunction)(Uint8*, Uint8);

#endif /* ENGINE_RENDERING_ENUMS */
