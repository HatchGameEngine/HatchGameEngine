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

enum {
    TILECOLLISION_NONE = 0,
    TILECOLLISION_DOWN = 1,
    TILECOLLISION_UP   = 2
};

enum {
    C_NONE      = 0,
    C_TOP       = 1,
    C_LEFT      = 2,
    C_RIGHT     = 3,
    C_BOTTOM    = 4
};

enum {
    FLIP_NONE   = 0,
    FLIP_X      = 1,
    FLIP_Y      = 2,
    FLIP_XY     = 3
};

enum {
    CMODE_FLOOR = 0,
    CMODE_LWALL = 1,
    CMODE_ROOF  = 2,
    CMODE_RWALL = 3
};

enum {
    ACTIVE_NEVER   = 0, // Never updates
    ACTIVE_ALWAYS  = 1, // Updates no matter what
    ACTIVE_NORMAL  = 2, // Updates no matter where the object is in the scene, but not if scene is paused
    ACTIVE_PAUSED  = 3, // Only updates when the scene is paused
    ACTIVE_BOUNDS  = 4, // Updates only when the object is within bounds
    ACTIVE_XBOUNDS = 5, // Updates within an x bound (not accounting for y bound)
    ACTIVE_YBOUNDS = 6, // Updates within a y bound (not accounting for x bound)
    ACTIVE_RBOUNDS = 7  // Updates within a radius (UpdateRegionW)
};

enum {
    SUNDAY      = 0,
    MONDAY      = 1,
    TUESDAY     = 2,
    WEDNESDAY   = 3,
    THURSDAY    = 4,
    FRIDAY      = 5,
    SATURDAY    = 6
};

enum {
    MORNING = 0, // Hours 5AM to 11AM. 0500 to 1100.
    MIDDAY  = 1, // Hours 12PM to 4PM. 1200 to 1600.
    EVENING = 2, // Hours 5PM to 8PM.  1700 to 2000.
    NIGHT   = 3  // Hours 9PM to 4AM.  2100 to 400.
};

#define TILE_FLIPX_MASK 0x80000000U
#define TILE_FLIPY_MASK 0x40000000U
// #define TILE_DIAGO_MASK 0x20000000U
#define TILE_COLLA_MASK 0x30000000U
#define TILE_COLLB_MASK 0x0C000000U
#define TILE_COLLC_MASK 0x03000000U
#define TILE_IDENT_MASK 0x00FFFFFFU

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

#endif /* ENGINE_RENDERING_ENUMS */
