#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Math/Clipper.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/Rendering/Software/Contour.h>
#include <Engine/Includes/HashMap.h>

class SoftwareRenderer {
public:
    static GraphicsFunctions BackendFunctions;
    static Uint32            CompareColor;
    static Sint32            CurrentPalette;
    static Uint32            CurrentArrayBuffer;
    static Uint32            PaletteColors[MAX_PALETTE_COUNT][0x100];
    static Uint8             PaletteIndexLines[MAX_FRAMEBUFFER_HEIGHT];
    static TileScanLine      TileScanLineBuffer[MAX_FRAMEBUFFER_HEIGHT];
    static Contour           ContourBuffer[MAX_FRAMEBUFFER_HEIGHT];
    static VertexBuffer*     VertexBuffers[MAX_VERTEX_BUFFERS];
    static Uint32            CurrentVertexBuffer;
};
#endif

#include <Engine/Rendering/Software/SoftwareRenderer.h>
#include <Engine/Rendering/Software/Rasterizer.h>
#include <Engine/Rendering/ArrayBuffer.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/ResourceStream.h>

#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/BytecodeObjectManager.h>

GraphicsFunctions SoftwareRenderer::BackendFunctions;
Uint32            SoftwareRenderer::CompareColor = 0xFF000000U;
Sint32            SoftwareRenderer::CurrentPalette = -1;
Uint32            SoftwareRenderer::CurrentArrayBuffer = -1;
Uint32            SoftwareRenderer::PaletteColors[MAX_PALETTE_COUNT][0x100];
Uint8             SoftwareRenderer::PaletteIndexLines[MAX_FRAMEBUFFER_HEIGHT];
TileScanLine      SoftwareRenderer::TileScanLineBuffer[MAX_FRAMEBUFFER_HEIGHT];
Contour           SoftwareRenderer::ContourBuffer[MAX_FRAMEBUFFER_HEIGHT];
VertexBuffer*     SoftwareRenderer::VertexBuffers[MAX_VERTEX_BUFFERS];
Uint32            SoftwareRenderer::CurrentVertexBuffer = -1;

enum BlendFlags {
    BlendFlag_OPAQUE = 0,
    BlendFlag_TRANSPARENT,
    BlendFlag_ADDITIVE,
    BlendFlag_SUBTRACT,
    BlendFlag_MATCH_EQUAL,
    BlendFlag_MATCH_NOT_EQUAL,
    BlendFlag_FILTER
};

int Alpha = 0xFF;
int BlendFlag = BlendFlag_OPAQUE;

Uint32 ColorAdd(Uint32 color1, Uint32 color2, int percent) {
	Uint32 r = (color1 & 0xFF0000U) + (((color2 & 0xFF0000U) * percent) >> 8);
	Uint32 g = (color1 & 0xFF00U) + (((color2 & 0xFF00U) * percent) >> 8);
	Uint32 b = (color1 & 0xFFU) + (((color2 & 0xFFU) * percent) >> 8);
    if (r > 0xFF0000U) r = 0xFF0000U;
	if (g > 0xFF00U) g = 0xFF00U;
	if (b > 0xFFU) b = 0xFFU;
	return r | g | b;
}
Uint32 ColorBlend(Uint32 color1, Uint32 color2, int percent) {
	Uint32 rb = color1 & 0xFF00FFU;
	Uint32 g  = color1 & 0x00FF00U;
	rb += (((color2 & 0xFF00FFU) - rb) * percent) >> 8;
	g  += (((color2 & 0x00FF00U) - g) * percent) >> 8;
	return (rb & 0xFF00FFU) | (g & 0x00FF00U);
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
int ColorMultiply(Uint32 color, Uint32 colorMult) {
    Uint32 R = (((colorMult >> 16) & 0xFF) + 1) * (color & 0xFF0000);
    Uint32 G = (((colorMult >> 8) & 0xFF) + 1) * (color & 0x00FF00);
    Uint32 B = (((colorMult) & 0xFF) + 1) * (color & 0x0000FF);
    return (int)((R >> 8) | (G >> 8) | (B >> 8));
}

#define CLAMP_VAL(v, a, b) if (v < a) v = a; else if (v > b) v = b;

// Utility functions
PUBLIC STATIC void    SoftwareRenderer::ConvertFromARGBtoNative(Uint32* argb, int count) {
    Uint8* color = (Uint8*)argb;
    if (Graphics::PreferredPixelFormat == SDL_PIXELFORMAT_ABGR8888) {
        for (int p = 0; p < count; p++) {
            *argb = 0xFF000000U | color[0] << 16 | color[1] << 8 | color[2];
            color += 4;
            argb++;
        }
    }
}

int ColR = 0xFF;
int ColG = 0xFF;
int ColB = 0xFF;
Uint32 ColRGB = 0xFFFFFF;
int MultTable[0x10000];
int MultTableInv[0x10000];
int MultSubTable[0x10000];

#define TRIG_TABLE_BITS 11
#define TRIG_TABLE_SIZE (1 << TRIG_TABLE_BITS)
#define TRIG_TABLE_MASK ((1 << TRIG_TABLE_BITS) - 1)
#define TRIG_TABLE_HALF (TRIG_TABLE_SIZE >> 1)

int SinTable[TRIG_TABLE_SIZE];
int CosTable[TRIG_TABLE_SIZE];

bool ClipPolygonsByFrustum = false;
int NumFrustumPlanes = 0;
Frustum ViewFrustum[NUM_FRUSTUM_PLANES];

bool DepthTest = false;
bool UseDepthBuffer = false;
size_t DepthBufferSize = 0;
Uint32* DepthBuffer = NULL;

float FogDensity = 1.0f;
int FogColor = 0x000000;

#define DEPTH_READ_DUMMY(depth) true
#define DEPTH_WRITE_DUMMY(depth)

#define DEPTH_READ_U32(depth) (depth < DepthBuffer[dst_x + dst_strideY])
#define DEPTH_WRITE_U32(depth) DepthBuffer[dst_x + dst_strideY] = depth

int FilterCurrent[0x8000];

int FilterColor[0x8000];
int FilterInvert[0x8000];
int FilterBlackAndWhite[0x8000];
int* FilterTable = NULL;
// Initialization and disposal functions
PUBLIC STATIC void     SoftwareRenderer::Init() {
    SoftwareRenderer::BackendFunctions.Init();
}
PUBLIC STATIC Uint32   SoftwareRenderer::GetWindowFlags() {
    return Graphics::Internal.GetWindowFlags();
}
PUBLIC STATIC void     SoftwareRenderer::SetGraphicsFunctions() {
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
        FilterColor[a] = hex | 0xFF000000U;

        // Game Boy-like Filter
        // bw = (((r & 0x10) + (g & 0x10) + (b & 0x10)) / 3) << 3;
        // bw <<= 1;
        // if (bw > 0xFF) bw = 0xFF;
        // r = bw * 183 / 255;
        // g = bw * 227 / 255;
        // b = bw * 42 / 255;
        // FilterBlackAndWhite[a] = b << 16 | g << 8 | r | 0xFF000000U;
    }

    FilterTable = &FilterColor[0];

    SoftwareRenderer::CurrentPalette = 0;
    for (int p = 0; p < MAX_PALETTE_COUNT; p++) {
        for (int c = 0; c < 0x100; c++) {
            SoftwareRenderer::PaletteColors[p][c]  = 0xFF000000U;
            SoftwareRenderer::PaletteColors[p][c] |= (c & 0x07) << 5; // Red?
            SoftwareRenderer::PaletteColors[p][c] |= (c & 0x18) << 11; // Green?
            SoftwareRenderer::PaletteColors[p][c] |= (c & 0xE0) << 16; // Blue?
        }
    }
    memset(SoftwareRenderer::PaletteIndexLines, 0, sizeof(SoftwareRenderer::PaletteIndexLines));

    SoftwareRenderer::BackendFunctions.Init = SoftwareRenderer::Init;
    SoftwareRenderer::BackendFunctions.GetWindowFlags = SoftwareRenderer::GetWindowFlags;
    SoftwareRenderer::BackendFunctions.Dispose = SoftwareRenderer::Dispose;

    // Texture management functions
    SoftwareRenderer::BackendFunctions.CreateTexture = SoftwareRenderer::CreateTexture;
    SoftwareRenderer::BackendFunctions.LockTexture = SoftwareRenderer::LockTexture;
    SoftwareRenderer::BackendFunctions.UpdateTexture = SoftwareRenderer::UpdateTexture;
    // SoftwareRenderer::BackendFunctions.UpdateYUVTexture = SoftwareRenderer::UpdateTextureYUV;
    SoftwareRenderer::BackendFunctions.UnlockTexture = SoftwareRenderer::UnlockTexture;
    SoftwareRenderer::BackendFunctions.DisposeTexture = SoftwareRenderer::DisposeTexture;

    // Viewport and view-related functions
    SoftwareRenderer::BackendFunctions.SetRenderTarget = SoftwareRenderer::SetRenderTarget;
    SoftwareRenderer::BackendFunctions.UpdateWindowSize = SoftwareRenderer::UpdateWindowSize;
    SoftwareRenderer::BackendFunctions.UpdateViewport = SoftwareRenderer::UpdateViewport;
    SoftwareRenderer::BackendFunctions.UpdateClipRect = SoftwareRenderer::UpdateClipRect;
    SoftwareRenderer::BackendFunctions.UpdateOrtho = SoftwareRenderer::UpdateOrtho;
    SoftwareRenderer::BackendFunctions.UpdatePerspective = SoftwareRenderer::UpdatePerspective;
    SoftwareRenderer::BackendFunctions.UpdateProjectionMatrix = SoftwareRenderer::UpdateProjectionMatrix;

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
    SoftwareRenderer::BackendFunctions.MakeFrameBufferID = SoftwareRenderer::MakeFrameBufferID;
}
PUBLIC STATIC void     SoftwareRenderer::Dispose() {
    SoftwareRenderer::BackendFunctions.Dispose();
}

PUBLIC STATIC void     SoftwareRenderer::RenderStart() {
    for (int i = 0; i < MAX_PALETTE_COUNT; i++)
        PaletteColors[i][0] &= 0xFFFFFF;
}
PUBLIC STATIC void     SoftwareRenderer::RenderEnd() {

}

// Texture management functions
PUBLIC STATIC Texture* SoftwareRenderer::CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
	Texture* texture = NULL; // Texture::New(format, access, width, height);

    return texture;
}
PUBLIC STATIC int      SoftwareRenderer::LockTexture(Texture* texture, void** pixels, int* pitch) {
    return 0;
}
PUBLIC STATIC int      SoftwareRenderer::UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch) {
    return 0;
}
PUBLIC STATIC int      SoftwareRenderer::UpdateYUVTexture(Texture* texture, SDL_Rect* src, Uint8* pixelsY, int pitchY, Uint8* pixelsU, int pitchU, Uint8* pixelsV, int pitchV) {
    return 0;
}
PUBLIC STATIC void     SoftwareRenderer::UnlockTexture(Texture* texture) {

}
PUBLIC STATIC void     SoftwareRenderer::DisposeTexture(Texture* texture) {

}

// Viewport and view-related functions
PUBLIC STATIC void     SoftwareRenderer::SetRenderTarget(Texture* texture) {

}
PUBLIC STATIC void     SoftwareRenderer::UpdateWindowSize(int width, int height) {
    Graphics::Internal.UpdateWindowSize(width, height);
}
PUBLIC STATIC void     SoftwareRenderer::UpdateViewport() {
    Graphics::Internal.UpdateViewport();
}
PUBLIC STATIC void     SoftwareRenderer::UpdateClipRect() {
    Graphics::Internal.UpdateClipRect();
}
PUBLIC STATIC void     SoftwareRenderer::UpdateOrtho(float left, float top, float right, float bottom) {
    Graphics::Internal.UpdateOrtho(left, top, right, bottom);
}
PUBLIC STATIC void     SoftwareRenderer::UpdatePerspective(float fovy, float aspect, float nearv, float farv) {
    Graphics::Internal.UpdatePerspective(fovy, aspect, nearv, farv);
}
PUBLIC STATIC void     SoftwareRenderer::UpdateProjectionMatrix() {
    Graphics::Internal.UpdateProjectionMatrix();
}

// Shader-related functions
PUBLIC STATIC void     SoftwareRenderer::UseShader(void* shader) {
    if (!shader) {
        FilterTable = &FilterColor[0];
        return;
    }

    ObjArray* array = (ObjArray*)shader;

    if (Graphics::PreferredPixelFormat == SDL_PIXELFORMAT_ARGB8888) {
        for (Uint32 i = 0, iSz = (Uint32)array->Values->size(); i < 0x8000 && i < iSz; i++) {
            FilterCurrent[i] = AS_INTEGER((*array->Values)[i]) | 0xFF000000U;
        }
    }
    else {
        Uint8 px[4];
        Uint32 newI;
        for (Uint32 i = 0, iSz = (Uint32)array->Values->size(); i < 0x8000 && i < iSz; i++) {
            *(Uint32*)&px[0] = AS_INTEGER((*array->Values)[i]);
            newI = (i & 0x1F) << 10 | (i & 0x3E0) | (i & 0x7C00) >> 10;
            FilterCurrent[newI] = px[0] << 16 | px[1] << 8 | px[2] | 0xFF000000U;
        }
    }
    FilterTable = &FilterCurrent[0];
}
PUBLIC STATIC void     SoftwareRenderer::SetUniformF(int location, int count, float* values) {

}
PUBLIC STATIC void     SoftwareRenderer::SetUniformI(int location, int count, int* values) {

}
PUBLIC STATIC void     SoftwareRenderer::SetUniformTexture(Texture* texture, int uniform_index, int slot) {

}

// These guys
PUBLIC STATIC void     SoftwareRenderer::Clear() {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;
    memset(dstPx, 0, dstStride * Graphics::CurrentRenderTarget->Height * 4);
}
PUBLIC STATIC void     SoftwareRenderer::Present() {

}

// Draw mode setting functions
PUBLIC STATIC void     SoftwareRenderer::SetBlendColor(float r, float g, float b, float a) {
    ColR = (int)(r * 0xFF);
    ColG = (int)(g * 0xFF);
    ColB = (int)(b * 0xFF);

    ColRGB = 0xFF000000U | ColR << 16 | ColG << 8 | ColB;
    SoftwareRenderer::ConvertFromARGBtoNative(&ColRGB, 1);

    if (a >= 1.0) {
        Alpha = 0xFF;
        // BlendFlag = BlendFlag_OPAQUE;
        return;
    }
    Alpha = (int)(a * 0xFF);
    // BlendFlag = BlendFlag_TRANSPARENT;
}
PUBLIC STATIC void     SoftwareRenderer::SetBlendMode(int srcC, int dstC, int srcA, int dstA) {
    switch (Graphics::BlendMode) {
        case BlendMode_NORMAL:
            BlendFlag = BlendFlag_TRANSPARENT;
            break;
        case BlendMode_ADD:
            BlendFlag = BlendFlag_ADDITIVE;
            break;
        case BlendMode_SUBTRACT:
            BlendFlag = BlendFlag_SUBTRACT;
            break;
        case BlendMode_MATCH_EQUAL:
            BlendFlag = BlendFlag_MATCH_EQUAL;
            break;
        case BlendMode_MATCH_NOT_EQUAL:
            BlendFlag = BlendFlag_MATCH_NOT_EQUAL;
            break;
    }
}

PUBLIC STATIC void     SoftwareRenderer::SetDepthTest(bool enabled) {
    DepthTest = enabled;

    if (DepthTest) {
        size_t dpSize = Graphics::CurrentRenderTarget->Width * Graphics::CurrentRenderTarget->Height;
        if (DepthBuffer == NULL) {
            DepthBufferSize = dpSize;
            InitDepthBuffer();
        }
        else if (dpSize != DepthBufferSize) {
            DepthBufferSize = dpSize;
            ResizeDepthBuffer();
        }

        ClearDepthBuffer();
    }
}
PUBLIC STATIC void     SoftwareRenderer::InitDepthBuffer() {
    DepthBuffer = (Uint32*)Memory::Calloc(DepthBufferSize, sizeof(*DepthBuffer));
}
PUBLIC STATIC void     SoftwareRenderer::ResizeDepthBuffer() {
    DepthBuffer = (Uint32*)Memory::Realloc(DepthBuffer, DepthBufferSize * sizeof(*DepthBuffer));
}
PUBLIC STATIC void     SoftwareRenderer::ClearDepthBuffer() {
    size_t size = Graphics::CurrentRenderTarget->Width * Graphics::CurrentRenderTarget->Height;
    if (DepthBuffer)
        memset(DepthBuffer, 0xFF, DepthBufferSize * sizeof(*DepthBuffer));
}

PUBLIC STATIC void     SoftwareRenderer::Resize(int width, int height) {

}

PUBLIC STATIC void     SoftwareRenderer::SetClip(float x, float y, float width, float height) {

}
PUBLIC STATIC void     SoftwareRenderer::ClearClip() {

}

PUBLIC STATIC void     SoftwareRenderer::Save() {

}
PUBLIC STATIC void     SoftwareRenderer::Translate(float x, float y, float z) {

}
PUBLIC STATIC void     SoftwareRenderer::Rotate(float x, float y, float z) {

}
PUBLIC STATIC void     SoftwareRenderer::Scale(float x, float y, float z) {

}
PUBLIC STATIC void     SoftwareRenderer::Restore() {

}

PUBLIC STATIC void     SoftwareRenderer::MakePerspectiveMatrix(Matrix4x4* out, float fov, float near, float far, float aspect) {
    float f = 1.0f / tanf(fov / 2.0f);
    float diff = near - far;

    out->Values[0]  = f / aspect;
    out->Values[1]  = 0.0f;
    out->Values[2]  = 0.0f;
    out->Values[3]  = 0.0f;

    out->Values[4]  = 0.0f;
    out->Values[5]  = -f;
    out->Values[6]  = 0.0f;
    out->Values[7]  = 0.0f;

    out->Values[8]  = 0.0f;
    out->Values[9]  = 0.0f;
    out->Values[10] = -(far + near) / diff;
    out->Values[11] = 1.0f;

    out->Values[12] = 0.0f;
    out->Values[13] = 0.0f;
    out->Values[14] = -(2.0f * far * near) / diff;
    out->Values[15] = 0.0f;
}

PUBLIC STATIC void     SoftwareRenderer::UnloadSceneData(void) {
    if (DepthBuffer) {
        Memory::Free(DepthBuffer);
        DepthBuffer = NULL;
    }

    for (Uint32 i = 0; i < MAX_VERTEX_BUFFERS; i++) {
        VertexBuffer* buffer = VertexBuffers[i];
        if (!buffer || buffer->UnloadPolicy > SCOPE_SCENE)
            continue;

        VertexBuffer_Delete(i);
    }
}

#define ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity) \
    if (FilterTable != &FilterColor[0]) \
        blendFlag = BlendFlag_FILTER; \
    if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT) \
        return; \
    else if (opacity != 0 && blendFlag == BlendFlag_OPAQUE) \
        blendFlag = BlendFlag_TRANSPARENT; \
    else if (opacity == 0xFF && blendFlag == BlendFlag_TRANSPARENT) \
        blendFlag = BlendFlag_OPAQUE;

// Filterless versions
#define GET_R(color) ((color >> 16) & 0xFF)
#define GET_G(color) ((color >> 8) & 0xFF)
#define GET_B(color) ((color) & 0xFF)
#define ISOLATE_R(color) (color & 0xFF0000)
#define ISOLATE_G(color) (color & 0x00FF00)
#define ISOLATE_B(color) (color & 0x0000FF)

void PixelNoFiltSetOpaque(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    *dst = *src;
}
void PixelNoFiltSetTransparent(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    int* multInvTableAt = &MultTableInv[opacity << 8];
    *dst = 0xFF000000U
        | (multTableAt[GET_R(*src)] + multInvTableAt[GET_R(*dst)]) << 16
        | (multTableAt[GET_G(*src)] + multInvTableAt[GET_G(*dst)]) << 8
        | (multTableAt[GET_B(*src)] + multInvTableAt[GET_B(*dst)]);
}
void PixelNoFiltSetAdditive(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    Uint32 R = (multTableAt[GET_R(*src)] << 16) + ISOLATE_R(*dst);
    Uint32 G = (multTableAt[GET_G(*src)] << 8) + ISOLATE_G(*dst);
    Uint32 B = (multTableAt[GET_B(*src)]) + ISOLATE_B(*dst);
    if (R > 0xFF0000) R = 0xFF0000;
    if (G > 0x00FF00) G = 0x00FF00;
    if (B > 0x0000FF) B = 0x0000FF;
    *dst = 0xFF000000U | R | G | B;
}
void PixelNoFiltSetSubtract(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    Sint32 R = (multSubTableAt[GET_R(*src)] << 16) + ISOLATE_R(*dst);
    Sint32 G = (multSubTableAt[GET_G(*src)] << 8) + ISOLATE_G(*dst);
    Sint32 B = (multSubTableAt[GET_B(*src)]) + ISOLATE_B(*dst);
    if (R < 0) R = 0;
    if (G < 0) G = 0;
    if (B < 0) B = 0;
    *dst = 0xFF000000U | R | G | B;
}
void PixelNoFiltSetMatchEqual(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    // if (*dst == SoftwareRenderer::CompareColor)
        // *dst = *src;
    if ((*dst & 0xFCFCFC) == (SoftwareRenderer::CompareColor & 0xFCFCFC))
        *dst = *src;
}
void PixelNoFiltSetMatchNotEqual(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    // if (*dst != SoftwareRenderer::CompareColor)
        // *dst = *src;
    if ((*dst & 0xFCFCFC) != (SoftwareRenderer::CompareColor & 0xFCFCFC))
        *dst = *src;
}
void PixelNoFiltSetFilter(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    Uint32 col = *dst;
    *dst = FilterTable[(col & 0xF80000) >> 9 | (col & 0xF800) >> 6 | (col & 0xF8) >> 3];
}

int TextureColorBlend(Uint32 color, Uint32 colorMult) {
    if ((colorMult & 0xFFFFFF) == 0xFFFFFF)
        return color;
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

int FogEquation(float density, float coord) {
    int result = expf(-density * coord) * 0x100;
    CLAMP_VAL(result, 0, 0x100);
    return 0x100 - result;
}
int DoFogLighting(int color, float fogCoord) {
    int fogValue = FogEquation(FogDensity, fogCoord / 192.0f);
    if (fogValue != 0)
        return ColorBlend(color, FogColor, fogValue);
    return color;
}
int LightPixel(int color, float depth) {
    return DoFogLighting(color, depth);
}
int LightPixelNoOp(int color, float depth) {
    return color;
}
int (*LightPixelFunc)(int, float) = NULL;

#define POLYGON_BLENDFLAGS(drawPolygonMacro, placePixelMacro) \
    switch (blendFlag) { \
        case BlendFlag_OPAQUE: \
            drawPolygonMacro(PixelNoFiltSetOpaque, placePixelMacro); \
            break; \
        case BlendFlag_TRANSPARENT: \
            drawPolygonMacro(PixelNoFiltSetTransparent, placePixelMacro); \
            break; \
        case BlendFlag_ADDITIVE: \
            drawPolygonMacro(PixelNoFiltSetAdditive, placePixelMacro); \
            break; \
        case BlendFlag_SUBTRACT: \
            drawPolygonMacro(PixelNoFiltSetSubtract, placePixelMacro); \
            break; \
        case BlendFlag_MATCH_EQUAL: \
            drawPolygonMacro(PixelNoFiltSetMatchEqual, placePixelMacro); \
            break; \
        case BlendFlag_MATCH_NOT_EQUAL: \
            drawPolygonMacro(PixelNoFiltSetMatchNotEqual, placePixelMacro); \
            break; \
        case BlendFlag_FILTER: \
            drawPolygonMacro(PixelNoFiltSetFilter, placePixelMacro); \
            break; \
    }

#define POLYGON_BLENDFLAGS_SOLID(drawPolygonMacro) \
    switch (blendFlag) { \
        case BlendFlag_OPAQUE: \
            drawPolygonMacro(PixelNoFiltSetOpaque); \
            break; \
        case BlendFlag_TRANSPARENT: \
            drawPolygonMacro(PixelNoFiltSetTransparent); \
            break; \
        case BlendFlag_ADDITIVE: \
            drawPolygonMacro(PixelNoFiltSetAdditive); \
            break; \
        case BlendFlag_SUBTRACT: \
            drawPolygonMacro(PixelNoFiltSetSubtract); \
            break; \
        case BlendFlag_MATCH_EQUAL: \
            drawPolygonMacro(PixelNoFiltSetMatchEqual); \
            break; \
        case BlendFlag_MATCH_NOT_EQUAL: \
            drawPolygonMacro(PixelNoFiltSetMatchNotEqual); \
            break; \
        case BlendFlag_FILTER: \
            drawPolygonMacro(PixelNoFiltSetFilter); \
            break; \
    }

#define POLYGON_BLENDFLAGS_SOLID_DEPTH(drawPolygonMacro, dpR, dpW) \
    switch (blendFlag) { \
        case BlendFlag_OPAQUE: \
            drawPolygonMacro(PixelNoFiltSetOpaque, dpR, dpW); \
            break; \
        case BlendFlag_TRANSPARENT: \
            drawPolygonMacro(PixelNoFiltSetTransparent, dpR, dpW); \
            break; \
        case BlendFlag_ADDITIVE: \
            drawPolygonMacro(PixelNoFiltSetAdditive, dpR, dpW); \
            break; \
        case BlendFlag_SUBTRACT: \
            drawPolygonMacro(PixelNoFiltSetSubtract, dpR, dpW); \
            break; \
        case BlendFlag_MATCH_EQUAL: \
            drawPolygonMacro(PixelNoFiltSetMatchEqual, dpR, dpW); \
            break; \
        case BlendFlag_MATCH_NOT_EQUAL: \
            drawPolygonMacro(PixelNoFiltSetMatchNotEqual, dpR, dpW); \
            break; \
        case BlendFlag_FILTER: \
            drawPolygonMacro(PixelNoFiltSetFilter, dpR, dpW); \
            break; \
    }

#define POLYGON_BLENDFLAGS_DEPTH(drawPolygonMacro, placePixelMacro, dpR, dpW) \
    switch (blendFlag) { \
        case BlendFlag_OPAQUE: \
            drawPolygonMacro(PixelNoFiltSetOpaque, placePixelMacro, dpR, dpW); \
            break; \
        case BlendFlag_TRANSPARENT: \
            drawPolygonMacro(PixelNoFiltSetTransparent, placePixelMacro, dpR, dpW); \
            break; \
        case BlendFlag_ADDITIVE: \
            drawPolygonMacro(PixelNoFiltSetAdditive, placePixelMacro, dpR, dpW); \
            break; \
        case BlendFlag_SUBTRACT: \
            drawPolygonMacro(PixelNoFiltSetSubtract, placePixelMacro, dpR, dpW); \
            break; \
        case BlendFlag_MATCH_EQUAL: \
            drawPolygonMacro(PixelNoFiltSetMatchEqual, placePixelMacro, dpR, dpW); \
            break; \
        case BlendFlag_MATCH_NOT_EQUAL: \
            drawPolygonMacro(PixelNoFiltSetMatchNotEqual, placePixelMacro, dpR, dpW); \
            break; \
        case BlendFlag_FILTER: \
            drawPolygonMacro(PixelNoFiltSetFilter, placePixelMacro, dpR, dpW); \
            break; \
    }

#define POLYGON_SCANLINE_DEPTH(drawPolygonMacro) \
    if (Graphics::UsePalettes && texture->Paletted) { \
        if (UseDepthBuffer) { \
            POLYGON_BLENDFLAGS_DEPTH(drawPolygonMacro, DRAW_PLACEPIXEL_PAL, DEPTH_READ_U32, DEPTH_WRITE_U32); \
        } \
        else { \
            POLYGON_BLENDFLAGS_DEPTH(drawPolygonMacro, DRAW_PLACEPIXEL_PAL, DEPTH_READ_DUMMY, DEPTH_WRITE_DUMMY); \
        } \
    } else { \
        if (UseDepthBuffer) { \
            POLYGON_BLENDFLAGS_DEPTH(drawPolygonMacro, DRAW_PLACEPIXEL, DEPTH_READ_U32, DEPTH_WRITE_U32); \
        } else {  \
            POLYGON_BLENDFLAGS_DEPTH(drawPolygonMacro, DRAW_PLACEPIXEL, DEPTH_READ_DUMMY, DEPTH_WRITE_DUMMY); \
        } \
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
    // contR += dxR * diff; \
    // contG += dxG * diff; \
    // contB += dxB * diff
#define SCANLINE_STEP_UV_BY(diff) \
    contU += dxU * diff; \
    contV += dxV * diff

#define SCANLINE_GET_MAPZ() \
    float mapZ = 1.0f / contZ; \
    Uint32 iz = mapZ * 0xFFFF
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
    int texU = ((int)(mapU * texture->Width) >> 16) % texture->Width; \
    int texV = ((int)(mapV * texture->Height) >> 16) % texture->Height
#define SCANLINE_GET_COLOR() \
    CLAMP_VAL(colR, 0x00, 0xFF0000); \
    CLAMP_VAL(colG, 0x00, 0xFF0000); \
    CLAMP_VAL(colB, 0x00, 0xFF0000); \
    col = ((colR) & 0xFF0000) | ((colG >> 8) & 0xFF00) | ((colB >> 16) & 0xFF)

Contour *ContourField = SoftwareRenderer::ContourBuffer;

// Draws a polygon
void DrawPolygon(Vector2* positions, Uint32 color, int count, int opacity, int blendFlag) {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;

    int minVal = INT_MAX;
    int maxVal = INT_MIN;

    if (count == 0)
        return;

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Vector2* tempVertex = positions;
    int      tempCount = count;
    int      tempY;
    while (tempCount--) {
        tempY = tempVertex->Y >> 16;
        if (minVal > tempY)
            minVal = tempY;
        if (maxVal < tempY)
            maxVal = tempY;
        tempVertex++;
    }

    int dst_y1 = minVal;
    int dst_y2 = maxVal;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    else {
        if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
            dst_y2 = (int)Graphics::CurrentRenderTarget->Height;
        if (dst_y1 < 0)
            dst_y1 = 0;
    }

    if (dst_y1 >= dst_y2)
        return;

    Rasterizer::Prepare(dst_y1, dst_y2);

    Vector2* lastVector = positions;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            Rasterizer::Scanline(lastVector[0].X, lastVector[0].Y, lastVector[1].X, lastVector[1].Y);
            lastVector++;
        }
    }
    Rasterizer::Scanline(lastVector[0].X, lastVector[0].Y, positions[0].X, positions[0].Y);

    #define DRAW_POLYGON(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        Contour contour = ContourField[dst_y]; \
        if (contour.MaxX < contour.MinX) { \
            dst_strideY += dstStride; \
            continue; \
        } \
        for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
            pixelFunction((Uint32*)&color, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
        } \
        dst_strideY += dstStride; \
    }

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;
    switch (blendFlag) {
        case BlendFlag_OPAQUE:
            for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
                Contour contour = ContourField[dst_y];
                if (contour.MaxX < contour.MinX) {
                    dst_strideY += dstStride;
                    continue;
                }

                Memory::Memset4(&dstPx[contour.MinX + dst_strideY], color, contour.MaxX - contour.MinX);
                dst_strideY += dstStride;
            }
            break;
        case BlendFlag_TRANSPARENT:
            DRAW_POLYGON(PixelNoFiltSetTransparent);
            break;
        case BlendFlag_ADDITIVE:
            DRAW_POLYGON(PixelNoFiltSetAdditive);
            break;
        case BlendFlag_SUBTRACT:
            DRAW_POLYGON(PixelNoFiltSetSubtract);
            break;
        case BlendFlag_MATCH_EQUAL:
            DRAW_POLYGON(PixelNoFiltSetMatchEqual);
            break;
        case BlendFlag_MATCH_NOT_EQUAL:
            DRAW_POLYGON(PixelNoFiltSetMatchNotEqual);
            break;
        case BlendFlag_FILTER:
            DRAW_POLYGON(PixelNoFiltSetFilter);
            break;
    }

    #undef DRAW_POLYGON
}
// Draws a blended polygon
void DrawPolygonBlend(Vector2* positions, int* colors, int count, int opacity, int blendFlag) {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;

    int minVal = INT_MAX;
    int maxVal = INT_MIN;

    if (count == 0)
        return;

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Vector2* tempVertex = positions;
    int      tempCount = count;
    int      tempY;
    while (tempCount--) {
        tempY = tempVertex->Y >> 16;
        if (minVal > tempY)
            minVal = tempY;
        if (maxVal < tempY)
            maxVal = tempY;
        tempVertex++;
    }

    int dst_y1 = minVal;
    int dst_y2 = maxVal;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
        dst_y2 = (int)Graphics::CurrentRenderTarget->Height;
    if (dst_y1 < 0)
        dst_y1 = 0;

    if (dst_y1 >= dst_y2)
        return;

    Rasterizer::Prepare(dst_y1, dst_y2);

    int* lastColor = colors;
    Vector2* lastVector = positions;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            Rasterizer::Scanline(lastColor[0], lastColor[1], lastVector[0].X, lastVector[0].Y, lastVector[1].X, lastVector[1].Y);
            lastVector++;
            lastColor++;
        }
    }

    Rasterizer::Scanline(lastColor[0], colors[0], lastVector[0].X, lastVector[0].Y, positions[0].X, positions[0].Y);

    Sint32 col, contLen;
    float contR, contG, contB, dxR, dxG, dxB;

    #define DRAW_POLYGONBLEND(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        Contour contour = ContourField[dst_y]; \
        contLen = contour.MaxX - contour.MinX; \
        if (contLen <= 0) { \
            dst_strideY += dstStride; \
            continue; \
        } \
        SCANLINE_INIT_RGB(); \
        if (contour.MapLeft < contour.MinX) { \
            int diff = contour.MinX - contour.MapLeft; \
            SCANLINE_STEP_RGB_BY(diff); \
        } \
        for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
            SCANLINE_GET_RGB(); \
            SCANLINE_GET_COLOR(); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            SCANLINE_STEP_RGB(); \
        } \
        dst_strideY += dstStride; \
    }

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;

    POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONBLEND);

    #undef DRAW_POLYGONBLEND
}
// Draws a polygon with lighting
void DrawPolygonShaded(Vector3* positions, Uint32 color, int count, int opacity, int blendFlag) {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;

    int minVal = INT_MAX;
    int maxVal = INT_MIN;

    if (count == 0)
        return;

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Vector3* tempVertex = positions;
    int      tempCount = count;
    int      tempY;
    while (tempCount--) {
        tempY = tempVertex->Y >> 16;
        if (minVal > tempY)
            minVal = tempY;
        if (maxVal < tempY)
            maxVal = tempY;
        tempVertex++;
    }

    int dst_y1 = minVal;
    int dst_y2 = maxVal;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
        dst_y2 = (int)Graphics::CurrentRenderTarget->Height;
    if (dst_y1 < 0)
        dst_y1 = 0;

    if (dst_y1 >= dst_y2)
        return;

    Rasterizer::Prepare(dst_y1, dst_y2);

    Vector3* lastVector = positions;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            Rasterizer::ScanlineDepth(lastVector[0].X, lastVector[0].Y, lastVector[0].Z, lastVector[1].X, lastVector[1].Y, lastVector[1].Z);
            lastVector++;
        }
    }

    Rasterizer::ScanlineDepth(lastVector[0].X, lastVector[0].Y, lastVector[0].Z, positions[0].X, positions[0].Y, positions[0].Z);

    Sint32 col, contLen;
    float contZ, dxZ;

    #define DRAW_POLYGONSHADED(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
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
            col = LightPixelFunc(color, mapZ); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            SCANLINE_STEP_Z(); \
        } \
        dst_strideY += dstStride; \
    }

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;

    POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONSHADED);

    #undef DRAW_POLYGONSHADED
}
// Draws a blended polygon with lighting
void DrawPolygonBlendShaded(Vector3* positions, int* colors, int count, int opacity, int blendFlag) {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;

    int minVal = INT_MAX;
    int maxVal = INT_MIN;

    if (count == 0)
        return;

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Vector3* tempVertex = positions;
    int      tempCount = count;
    int      tempY;
    while (tempCount--) {
        tempY = tempVertex->Y >> 16;
        if (minVal > tempY)
            minVal = tempY;
        if (maxVal < tempY)
            maxVal = tempY;
        tempVertex++;
    }

    int dst_y1 = minVal;
    int dst_y2 = maxVal;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
        dst_y2 = (int)Graphics::CurrentRenderTarget->Height;
    if (dst_y1 < 0)
        dst_y1 = 0;

    if (dst_y1 >= dst_y2)
        return;

    Rasterizer::Prepare(dst_y1, dst_y2);

    int* lastColor = colors;
    Vector3* lastVector = positions;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            Rasterizer::ScanlineDepth(lastColor[0], lastColor[1], lastVector[0].X, lastVector[0].Y, lastVector[0].Z, lastVector[1].X, lastVector[1].Y, lastVector[1].Z);
            lastVector++;
            lastColor++;
        }
    }

    Rasterizer::ScanlineDepth(lastColor[0], colors[0], lastVector[0].X, lastVector[0].Y, lastVector[0].Z, positions[0].X, positions[0].Y, positions[0].Z);

    Sint32 col, contLen;
    float contZ, contR, contG, contB;
    float dxZ, dxR, dxG, dxB;

    #define DRAW_POLYGONBLENDSHADED(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
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
            SCANLINE_GET_COLOR(); \
            col = LightPixelFunc(col, mapZ); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            SCANLINE_STEP_Z(); \
            SCANLINE_STEP_RGB(); \
        } \
        dst_strideY += dstStride; \
    }

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;

    POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONBLENDSHADED);

    #undef DRAW_POLYGONBLENDSHADED
}
// Draws an affine texture mapped polygon
void DrawPolygonAffine(Texture* texture, Vector3* positions, Vector2* uvs, Uint32 color, int count, int opacity, int blendFlag) {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;
    Uint32* srcPx = (Uint32*)texture->Pixels;
    Uint32  srcStride = texture->Width;

    int minVal = INT_MAX;
    int maxVal = INT_MIN;

    if (count == 0)
        return;

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Vector3* tempVertex = positions;
    int      tempCount = count;
    int      tempY;
    while (tempCount--) {
        tempY = tempVertex->Y >> 16;
        if (minVal > tempY)
            minVal = tempY;
        if (maxVal < tempY)
            maxVal = tempY;
        tempVertex++;
    }

    int dst_y1 = minVal;
    int dst_y2 = maxVal;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    else {
        if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
            dst_y2 = (int)Graphics::CurrentRenderTarget->Height;
        if (dst_y1 < 0)
            dst_y1 = 0;
    }

    if (dst_y1 >= dst_y2)
        return;

    Rasterizer::Prepare(dst_y1, dst_y2);

    Vector3* lastVector = positions;
    Vector2* lastUV = uvs;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            Rasterizer::ScanlineUVAffine(lastUV[0], lastUV[1], lastVector[0].X, lastVector[0].Y, lastVector[0].Z, lastVector[1].X, lastVector[1].Y, lastVector[1].Z);
            lastVector++;
            lastUV++;
        }
    }

    Rasterizer::ScanlineUVAffine(lastUV[0], uvs[0], lastVector[0].X, lastVector[0].Y, lastVector[0].Z, positions[0].X, positions[0].Y, positions[0].Z);

    Sint32 col, texCol, contLen;
    float contZ, contU, contV;
    float dxZ, dxU, dxV;

    #define DRAW_PLACEPIXEL(pixelFunction, dpW) \
        if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
            col = TextureColorBlend(color, texCol); \
            col = LightPixelFunc(col, mapZ); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            dpW(iz); \
        }

    #define DRAW_PLACEPIXEL_PAL(pixelFunction, dpW) \
        if ((texCol = srcPx[(texV * srcStride) + texU])) { \
            col = TextureColorBlend(color, index[texCol]); \
            col = LightPixelFunc(col, mapZ); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            dpW(iz); \
        } \

    #define DRAW_POLYGONAFFINE(pixelFunction, placePixelMacro, dpR, dpW) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        Contour contour = ContourField[dst_y]; \
        contLen = contour.MaxX - contour.MinX; \
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
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
            SCANLINE_GET_MAPZ(); \
            if (dpR(iz)) { \
                SCANLINE_GET_MAPUV(); \
                SCANLINE_GET_TEXUV(); \
                placePixelMacro(pixelFunction, dpW); \
            } \
            SCANLINE_STEP_Z(); \
            SCANLINE_STEP_UV(); \
        } \
        dst_strideY += dstStride; \
    }

    Uint32* index;
    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;

    POLYGON_SCANLINE_DEPTH(DRAW_POLYGONAFFINE);

    #undef DRAW_PLACEPIXEL
    #undef DRAW_PLACEPIXEL_PAL
    #undef DRAW_POLYGONAFFINE
}
// Draws an affine texture mapped polygon with blending
void DrawPolygonBlendAffine(Texture* texture, Vector3* positions, Vector2* uvs, int* colors, int count, int opacity, int blendFlag) {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;
    Uint32* srcPx = (Uint32*)texture->Pixels;
    Uint32  srcStride = texture->Width;

    int minVal = INT_MAX;
    int maxVal = INT_MIN;

    if (count == 0)
        return;

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Vector3* tempVertex = positions;
    int      tempCount = count;
    int      tempY;
    while (tempCount--) {
        tempY = tempVertex->Y >> 16;
        if (minVal > tempY)
            minVal = tempY;
        if (maxVal < tempY)
            maxVal = tempY;
        tempVertex++;
    }

    int dst_y1 = minVal;
    int dst_y2 = maxVal;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    else {
        if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
            dst_y2 = (int)Graphics::CurrentRenderTarget->Height;
        if (dst_y1 < 0)
            dst_y1 = 0;
    }

    if (dst_y1 >= dst_y2)
        return;

    Rasterizer::Prepare(dst_y1, dst_y2);

    int*     lastColor = colors;
    Vector3* lastPosition = positions;
    Vector2* lastUV = uvs;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            Rasterizer::ScanlineUVAffine(lastColor[0], lastColor[1], lastUV[0], lastUV[1], lastPosition[0].X, lastPosition[0].Y, lastPosition[0].Z, lastPosition[1].X, lastPosition[1].Y, lastPosition[1].Z);
            lastPosition++;
            lastColor++;
            lastUV++;
        }
    }

    Rasterizer::ScanlineUVAffine(lastColor[0], colors[0], lastUV[0], uvs[0], lastPosition[0].X, lastPosition[0].Y, lastPosition[0].Z, positions[0].X, positions[0].Y, positions[0].Z);

    Sint32 col, texCol, contLen;
    float contZ, contU, contV, contR, contG, contB;
    float dxZ, dxU, dxV, dxR, dxG, dxB;

    #define DRAW_PLACEPIXEL(pixelFunction, dpW) \
        if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
            SCANLINE_GET_COLOR(); \
            col = TextureColorBlend(col, texCol); \
            col = LightPixelFunc(col, mapZ); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            dpW(iz); \
        }

    #define DRAW_PLACEPIXEL_PAL(pixelFunction, dpW) \
        if ((texCol = srcPx[(texV * srcStride) + texU])) { \
            SCANLINE_GET_COLOR(); \
            col = TextureColorBlend(col, index[texCol]); \
            col = LightPixelFunc(col, mapZ); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            dpW(iz); \
        } \

    #define DRAW_POLYGONBLENDAFFINE(pixelFunction, placePixelMacro, dpR, dpW) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        Contour contour = ContourField[dst_y]; \
        contLen = contour.MaxX - contour.MinX; \
        if (contLen <= 0) { \
            dst_strideY += dstStride; \
            continue; \
        } \
        SCANLINE_INIT_Z(); \
        SCANLINE_INIT_UV(); \
        SCANLINE_INIT_RGB(); \
        if (contour.MapLeft < contour.MinX) { \
            int diff = contour.MinX - contour.MapLeft; \
            SCANLINE_STEP_Z_BY(diff); \
            SCANLINE_STEP_UV_BY(diff); \
            SCANLINE_STEP_RGB_BY(diff); \
        } \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
            SCANLINE_GET_MAPZ(); \
            if (dpR(iz)) { \
                SCANLINE_GET_RGB(); \
                SCANLINE_GET_MAPUV(); \
                SCANLINE_GET_TEXUV(); \
                placePixelMacro(pixelFunction, dpW); \
            } \
            SCANLINE_STEP_Z(); \
            SCANLINE_STEP_UV(); \
            SCANLINE_STEP_RGB(); \
        } \
        dst_strideY += dstStride; \
    }

    Uint32* index;
    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;

    POLYGON_SCANLINE_DEPTH(DRAW_POLYGONBLENDAFFINE);

    #undef DRAW_PLACEPIXEL
    #undef DRAW_PLACEPIXEL_PAL
    #undef DRAW_POLYGONBLENDAFFINE
    #undef BLENDFLAGS
}
// Draws a perspective-correct texture mapped polygon
#ifdef SCANLINE_DIVISION
#define PERSP_MAPPER_SUBDIVIDE(width, placePixelMacro, pixelFunction, dpR, dpW) \
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
        Uint32 iz = mapZ * 0xFFFF; \
        if (dpR(iz)) { \
            DRAW_PERSP_GET(currU, currV); \
            placePixelMacro(pixelFunction, dpW); \
        } \
        DRAW_PERSP_STEP(); \
        mapZ += stepZ; \
        currU += stepU; \
        currV += stepV; \
        dst_x++; \
    }
#define DO_PERSP_MAPPING(placePixelMacro, pixelFunction, dpR, dpW) { \
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
        PERSP_MAPPER_SUBDIVIDE(contSize, placePixelMacro, pixelFunction, dpR, dpW); \
        startU = endU; \
        startV = endV; \
        startZ = endZ; \
        contWidth -= contSize; \
    } \
    if (contWidth > 0) { \
        if (contWidth > 1) { \
            PERSP_MAPPER_SUBDIVIDE(contWidth, placePixelMacro, pixelFunction, dpR, dpW); \
        } \
        else { \
            Uint32 iz = startZ * 0xFFFF; \
            if (dpR(iz)) { \
                DRAW_PERSP_GET(startU, startV); \
                placePixelMacro(pixelFunction, dpW); \
            } \
        } \
    } \
}
#else
#define DO_PERSP_MAPPING(placePixelMacro, pixelFunction, dpR, dpW) \
    for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
        SCANLINE_GET_MAPZ(); \
        if (dpR(iz)) { \
            DRAW_PERSP_GET(contU * mapZ, contV * mapZ); \
            placePixelMacro(pixelFunction, dpW); \
        } \
        SCANLINE_STEP_Z(); \
        SCANLINE_STEP_UV(); \
        DRAW_PERSP_STEP(); \
    }
#endif
void DrawPolygonPerspective(Texture* texture, Vector3* positions, Vector2* uvs, Uint32 color, int count, int opacity, int blendFlag) {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;
    Uint32* srcPx = (Uint32*)texture->Pixels;
    Uint32  srcStride = texture->Width;

    int minVal = INT_MAX;
    int maxVal = INT_MIN;

    if (count == 0)
        return;

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Vector3* tempVertex = positions;
    int      tempCount = count;
    int      tempY;
    while (tempCount--) {
        tempY = tempVertex->Y >> 16;
        if (minVal > tempY)
            minVal = tempY;
        if (maxVal < tempY)
            maxVal = tempY;
        tempVertex++;
    }

    int dst_y1 = minVal;
    int dst_y2 = maxVal;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    else {
        if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
            dst_y2 = (int)Graphics::CurrentRenderTarget->Height;
        if (dst_y1 < 0)
            dst_y1 = 0;
    }

    if (dst_y1 >= dst_y2)
        return;

    Rasterizer::Prepare(dst_y1, dst_y2);

    Vector3* lastVector = positions;
    Vector2* lastUV = uvs;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            Rasterizer::ScanlineUV(lastUV[0], lastUV[1], lastVector[0].X, lastVector[0].Y, lastVector[0].Z, lastVector[1].X, lastVector[1].Y, lastVector[1].Z);
            lastVector++;
            lastUV++;
        }
    }

    Rasterizer::ScanlineUV(lastUV[0], uvs[0], lastVector[0].X, lastVector[0].Y, lastVector[0].Z, positions[0].X, positions[0].Y, positions[0].Z);

    Sint32 col, texCol, contLen;
    float contZ, contU, contV;
    float dxZ, dxU, dxV;

    #define DRAW_PLACEPIXEL(pixelFunction, dpW) \
        if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
            col = TextureColorBlend(color, texCol); \
            col = LightPixelFunc(col, mapZ); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            dpW(iz); \
        }

    #define DRAW_PLACEPIXEL_PAL(pixelFunction, dpW) \
        if ((texCol = srcPx[(texV * srcStride) + texU])) { \
            col = TextureColorBlend(color, index[texCol]); \
            col = LightPixelFunc(col, mapZ); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            dpW(iz); \
        } \

    #define DRAW_PERSP_GET(u, v) \
        float mapU = u; \
        float mapV = v; \
        SCANLINE_GET_TEXUV()

    #define DRAW_PERSP_STEP()

    #define DRAW_POLYGONPERSP(pixelFunction, placePixelMacro, dpR, dpW) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
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
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        DO_PERSP_MAPPING(placePixelMacro, pixelFunction, dpR, dpW); \
        dst_strideY += dstStride; \
    }

    Uint32* index;
    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;

    POLYGON_SCANLINE_DEPTH(DRAW_POLYGONPERSP);

    #undef DRAW_PLACEPIXEL
    #undef DRAW_PLACEPIXEL_PAL
    #undef DRAW_PERSP_GET
    #undef DRAW_PERSP_STEP
    #undef DRAW_POLYGONPERSP
}
// Draws a perspective-correct texture mapped polygon with blending
void DrawPolygonBlendPerspective(Texture* texture, Vector3* positions, Vector2* uvs, int* colors, int count, int opacity, int blendFlag) {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;
    Uint32* srcPx = (Uint32*)texture->Pixels;
    Uint32  srcStride = texture->Width;

    int minVal = INT_MAX;
    int maxVal = INT_MIN;

    if (count == 0)
        return;

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Vector3* tempVertex = positions;
    int      tempCount = count;
    int      tempY;
    while (tempCount--) {
        tempY = tempVertex->Y >> 16;
        if (minVal > tempY)
            minVal = tempY;
        if (maxVal < tempY)
            maxVal = tempY;
        tempVertex++;
    }

    int dst_y1 = minVal;
    int dst_y2 = maxVal;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    else {
        if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
            dst_y2 = (int)Graphics::CurrentRenderTarget->Height;
        if (dst_y1 < 0)
            dst_y1 = 0;
    }

    if (dst_y1 >= dst_y2)
        return;

    Rasterizer::Prepare(dst_y1, dst_y2);

    int*     lastColor = colors;
    Vector3* lastPosition = positions;
    Vector2* lastUV = uvs;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            Rasterizer::ScanlineUV(lastColor[0], lastColor[1], lastUV[0], lastUV[1], lastPosition[0].X, lastPosition[0].Y, lastPosition[0].Z, lastPosition[1].X, lastPosition[1].Y, lastPosition[1].Z);
            lastPosition++;
            lastColor++;
            lastUV++;
        }
    }

    Rasterizer::ScanlineUV(lastColor[0], colors[0], lastUV[0], uvs[0], lastPosition[0].X, lastPosition[0].Y, lastPosition[0].Z, positions[0].X, positions[0].Y, positions[0].Z);

    Sint32 col, texCol, contLen;
    float contZ, contU, contV, contR, contG, contB;
    float dxZ, dxU, dxV, dxR, dxG, dxB;

    #define DRAW_PLACEPIXEL(pixelFunction, dpW) \
        if ((texCol = srcPx[(texV * srcStride) + texU]) & 0xFF000000U) { \
            SCANLINE_GET_COLOR(); \
            col = TextureColorBlend(col, texCol); \
            col = LightPixelFunc(col, mapZ); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            dpW(iz); \
        }

    #define DRAW_PLACEPIXEL_PAL(pixelFunction, dpW) \
        if ((texCol = srcPx[(texV * srcStride) + texU])) { \
            SCANLINE_GET_COLOR(); \
            col = TextureColorBlend(col, index[texCol]); \
            col = LightPixelFunc(col, mapZ); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            dpW(iz); \
        }

    #define DRAW_PERSP_GET(u, v) \
        float mapU = u; \
        float mapV = v; \
        SCANLINE_GET_RGB_PERSP(); \
        SCANLINE_GET_TEXUV()

    #define DRAW_PERSP_STEP() SCANLINE_STEP_RGB()

    #define DRAW_POLYGONBLENDPERSP(pixelFunction, placePixelMacro, dpR, dpW) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
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
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        DO_PERSP_MAPPING(placePixelMacro, pixelFunction, dpR, dpW); \
        dst_strideY += dstStride; \
    }

    Uint32* index;
    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;

    POLYGON_SCANLINE_DEPTH(DRAW_POLYGONBLENDPERSP);

    #undef DRAW_PLACEPIXEL
    #undef DRAW_PLACEPIXEL_PAL
    #undef DRAW_PERSP_GET
    #undef DRAW_PERSP_STEP
    #undef DRAW_POLYGONBLENDPERSP
}
// Draws a polygon with depth testing
void DrawPolygonDepth(Vector3* positions, Uint32 color, int count, int opacity, int blendFlag) {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;

    int minVal = INT_MAX;
    int maxVal = INT_MIN;

    if (count == 0)
        return;

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Vector3* tempVertex = positions;
    int      tempCount = count;
    int      tempY;
    while (tempCount--) {
        tempY = tempVertex->Y >> 16;
        if (minVal > tempY)
            minVal = tempY;
        if (maxVal < tempY)
            maxVal = tempY;
        tempVertex++;
    }

    int dst_y1 = minVal;
    int dst_y2 = maxVal;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
        dst_y2 = (int)Graphics::CurrentRenderTarget->Height;
    if (dst_y1 < 0)
        dst_y1 = 0;

    if (dst_y1 >= dst_y2)
        return;

    Rasterizer::Prepare(dst_y1, dst_y2);

    Vector3* lastVector = positions;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            Rasterizer::ScanlineDepth(lastVector[0].X, lastVector[0].Y, lastVector[0].Z, lastVector[1].X, lastVector[1].Y, lastVector[1].Z);
            lastVector++;
        }
    }

    Rasterizer::ScanlineDepth(lastVector[0].X, lastVector[0].Y, lastVector[0].Z, positions[0].X, positions[0].Y, positions[0].Z);

    Sint32 col, contLen;
    float contZ, dxZ;

    #define DRAW_POLYGONDEPTH(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
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
            if (DEPTH_READ_U32(iz)) { \
                col = LightPixelFunc(color, mapZ); \
                pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
                DEPTH_WRITE_U32(iz); \
            } \
            SCANLINE_STEP_Z(); \
        } \
        dst_strideY += dstStride; \
    }

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;

    POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONDEPTH);

    #undef DRAW_POLYGONDEPTH
}
// Draws a blended polygon with depth testing
void DrawPolygonBlendDepth(Vector3* positions, int* colors, int count, int opacity, int blendFlag) {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;

    int minVal = INT_MAX;
    int maxVal = INT_MIN;

    if (count == 0)
        return;

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Vector3* tempVertex = positions;
    int      tempCount = count;
    int      tempY;
    while (tempCount--) {
        tempY = tempVertex->Y >> 16;
        if (minVal > tempY)
            minVal = tempY;
        if (maxVal < tempY)
            maxVal = tempY;
        tempVertex++;
    }

    int dst_y1 = minVal;
    int dst_y2 = maxVal;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
        dst_y2 = (int)Graphics::CurrentRenderTarget->Height;
    if (dst_y1 < 0)
        dst_y1 = 0;

    if (dst_y1 >= dst_y2)
        return;

    Rasterizer::Prepare(dst_y1, dst_y2);

    int* lastColor = colors;
    Vector3* lastVector = positions;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            Rasterizer::ScanlineDepth(lastColor[0], lastColor[1], lastVector[0].X, lastVector[0].Y, lastVector[0].Z, lastVector[1].X, lastVector[1].Y, lastVector[1].Z);
            lastVector++;
            lastColor++;
        }
    }

    Rasterizer::ScanlineDepth(lastColor[0], colors[0], lastVector[0].X, lastVector[0].Y, lastVector[0].Z, positions[0].X, positions[0].Y, positions[0].Z);

    Sint32 col, contLen;
    float contZ, contR, contG, contB;
    float dxZ, dxR, dxG, dxB;

    #define DRAW_POLYGONBLENDDEPTH(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
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
            if (DEPTH_READ_U32(iz)) { \
                SCANLINE_GET_RGB(); \
                SCANLINE_GET_COLOR(); \
                col = LightPixelFunc(col, mapZ); \
                pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
                DEPTH_WRITE_U32(iz); \
            } \
            SCANLINE_STEP_Z(); \
            SCANLINE_STEP_RGB(); \
        } \
        dst_strideY += dstStride; \
    }

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;

    POLYGON_BLENDFLAGS_SOLID(DRAW_POLYGONBLENDDEPTH);

    #undef DRAW_POLYGONBLENDDEPTH
}

// TODO: Material support
static int CalcVertexColor(ArrayBuffer* arrayBuffer, VertexAttribute *vertex, int normalY) {
    int col_r = GET_R(vertex->Color);
    int col_g = GET_G(vertex->Color);
    int col_b = GET_B(vertex->Color);
    int specularR = 0, specularG = 0, specularB = 0;

    int ambientNormalY = normalY >> 10;
    int reweightedNormal = (normalY >> 2) * (abs(normalY) >> 2);

    // r
    col_r = (col_r * (ambientNormalY + arrayBuffer->LightingAmbientR)) >> arrayBuffer->LightingDiffuseR;
    specularR = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularR;
    CLAMP_VAL(specularR, 0x00, 0xFF);
    specularR += col_r;
    CLAMP_VAL(specularR, 0x00, 0xFF);
    col_r = specularR;

    // g
    col_g = (col_g * (ambientNormalY + arrayBuffer->LightingAmbientG)) >> arrayBuffer->LightingDiffuseG;
    specularG = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularG;
    CLAMP_VAL(specularG, 0x00, 0xFF);
    specularG += col_g;
    CLAMP_VAL(specularG, 0x00, 0xFF);
    col_g = specularG;

    // b
    col_b = (col_b * (ambientNormalY + arrayBuffer->LightingAmbientB)) >> arrayBuffer->LightingDiffuseB;
    specularB = reweightedNormal >> 6 >> arrayBuffer->LightingSpecularB;
    CLAMP_VAL(specularB, 0x00, 0xFF);
    specularB += col_b;
    CLAMP_VAL(specularB, 0x00, 0xFF);
    col_b = specularB;

    return col_r << 16 | col_g << 8 | col_b;
}

static int SortPolygonFaces(const void *a, const void *b) {
    const FaceInfo* faceA = (const FaceInfo *)a;
    const FaceInfo* faceB = (const FaceInfo *)b;
    return faceB->Depth - faceA->Depth;
}

static void BuildFrustumPlanes(ArrayBuffer* arrayBuffer, Frustum* frustum) {
    ClipPolygonsByFrustum = arrayBuffer->ClipPolygons;
    if (!ClipPolygonsByFrustum)
        return;

    // Near
    frustum[0].Plane.Z = arrayBuffer->NearClippingPlane * 0x10000;
    frustum[0].Normal.Z = 0x10000;

    // Far
    frustum[1].Plane.Z = arrayBuffer->FarClippingPlane * 0x10000;
    frustum[1].Normal.Z = -0x10000;

    NumFrustumPlanes = 2;
}

// Drawing 3D
ArrayBuffer ArrayBuffers[MAX_ARRAY_BUFFERS];
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_Init(Uint32 arrayBufferIndex, Uint32 maxVertices) {
    if (arrayBufferIndex < 0 || arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;

    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    VertexBuffer::Init(&arrayBuffer->Buffer, maxVertices);

    arrayBuffer->Initialized = true;
    arrayBuffer->ClipPolygons = true;

    SoftwareRenderer::ArrayBuffer_InitMatrices(arrayBufferIndex);
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_InitMatrices(Uint32 arrayBufferIndex) {
    Matrix4x4 projMat, viewMat;
    SoftwareRenderer::MakePerspectiveMatrix(&projMat, 90.0f * M_PI / 180.0f, 1.0f, 32768.0f, 1.0f);
    Matrix4x4::Identity(&viewMat);

    SoftwareRenderer::ArrayBuffer_SetProjectionMatrix(arrayBufferIndex, &projMat);
    SoftwareRenderer::ArrayBuffer_SetViewMatrix(arrayBufferIndex, &viewMat);
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_SetAmbientLighting(Uint32 arrayBufferIndex, Uint32 r, Uint32 g, Uint32 b) {
    if (arrayBufferIndex < 0 || arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;
    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->LightingAmbientR = r;
    arrayBuffer->LightingAmbientG = g;
    arrayBuffer->LightingAmbientB = b;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_SetDiffuseLighting(Uint32 arrayBufferIndex, Uint32 r, Uint32 g, Uint32 b) {
    if (arrayBufferIndex < 0 || arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;
    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->LightingDiffuseR = r;
    arrayBuffer->LightingDiffuseG = g;
    arrayBuffer->LightingDiffuseB = b;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_SetSpecularLighting(Uint32 arrayBufferIndex, Uint32 r, Uint32 g, Uint32 b) {
    if (arrayBufferIndex < 0 || arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;
    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->LightingSpecularR = r;
    arrayBuffer->LightingSpecularG = g;
    arrayBuffer->LightingSpecularB = b;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_SetFogDensity(Uint32 arrayBufferIndex, float density) {
    if (arrayBufferIndex < 0 || arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;
    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->FogDensity = density;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_SetFogColor(Uint32 arrayBufferIndex, Uint32 r, Uint32 g, Uint32 b) {
    if (arrayBufferIndex < 0 || arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;
    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->FogColorR = r;
    arrayBuffer->FogColorG = g;
    arrayBuffer->FogColorB = b;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_SetClipPolygons(Uint32 arrayBufferIndex, bool clipPolygons) {
    if (arrayBufferIndex < 0 || arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;
    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->ClipPolygons = clipPolygons;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_Bind(Uint32 arrayBufferIndex) {
    CurrentArrayBuffer = arrayBufferIndex;
    ArrayBuffer_DrawBegin(arrayBufferIndex);
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_DrawBegin(Uint32 arrayBufferIndex) {
    if (arrayBufferIndex < 0 || arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;

    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (arrayBuffer->Initialized) {
        VertexBuffer::Clear(&arrayBuffer->Buffer);
        BuildFrustumPlanes(arrayBuffer, ViewFrustum);
    }
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_SetProjectionMatrix(Uint32 arrayBufferIndex, Matrix4x4* projMat) {
    if (arrayBufferIndex < 0 || arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;

    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->ProjectionMatrix  = *projMat;
    arrayBuffer->FarClippingPlane  = projMat->Values[14] / (projMat->Values[10] - 1.0f);
    arrayBuffer->NearClippingPlane = projMat->Values[14] / (projMat->Values[10] + 1.0f);
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_SetViewMatrix(Uint32 arrayBufferIndex, Matrix4x4* viewMat) {
    if (arrayBufferIndex < 0 || arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;

    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->ViewMatrix = *viewMat;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_DrawFinish(Uint32 arrayBufferIndex, Uint32 drawMode) {
    if (arrayBufferIndex < 0 || arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;

    int vertexCount, vertexCountPerFace, vertexCountPerFaceMinus1;
    FaceInfo* faceInfoPtr;
    ArrayBuffer* arrayBuffer;
    VertexBuffer* vertexBuffer;
    VertexAttribute* vertexAttribsPtr;

    View* currentView = &Scene::Views[Scene::ViewCurrent];
    int cx = (int)std::floor(currentView->X);
    int cy = (int)std::floor(currentView->Y);

    Matrix4x4* out = Graphics::ModelViewMatrix.top();
    int x = out->Values[12];
    int y = out->Values[13];
    x -= cx;
    y -= cy;

    int opacity;
    int blendFlag;

#define SET_BLENDFLAG_AND_OPACITY(face) \
    if (!Graphics::TextureBlend) { \
        blendFlag = BlendFlag_OPAQUE; \
        opacity = 0xFF; \
    } else { \
        blendFlag = face->BlendFlag; \
        opacity = face->Opacity; \
        if (blendFlag == BlendFlag_TRANSPARENT && opacity == 0xFF) \
            blendFlag = BlendFlag_OPAQUE; \
    } \
    if (blendFlag != BlendFlag_OPAQUE) \
        UseDepthBuffer = false; \
    else \
        UseDepthBuffer = DepthTest

    arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->Initialized)
        return;
    vertexBuffer = &arrayBuffer->Buffer;

    SoftwareRenderer::SetDepthTest(drawMode & DrawMode_DEPTH_TEST);

    if (drawMode & DrawMode_FOG) {
        FogColor       = arrayBuffer->FogColorR << 16 | arrayBuffer->FogColorG << 8 | arrayBuffer->FogColorB;
        FogDensity     = arrayBuffer->FogDensity;
        LightPixelFunc = LightPixel;
    }
    else
        LightPixelFunc = LightPixelNoOp;

    vertexAttribsPtr = vertexBuffer->Vertices; // R
    faceInfoPtr = vertexBuffer->FaceInfoBuffer; // RW

    bool doAffineMapping = drawMode & DrawMode_AFFINE;
    bool usePerspective = !(drawMode & DrawMode_ORTHOGRAPHIC);
    bool sortFaces = !DepthTest && vertexBuffer->FaceCount > 1;

    if (Graphics::TextureBlend)
        sortFaces = true;

    // Get the face depth and vertices' start index
    int verticesStartIndex = 0;
    for (int f = 0; f < vertexBuffer->FaceCount; f++) {
        vertexCount = faceInfoPtr->NumVertices;

        // Average the Z coordinates of the face
        if (sortFaces) {
            Sint64 depth = vertexAttribsPtr[0].Position.Z;
            for (int i = 1; i < vertexCount; i++)
                depth += vertexAttribsPtr[i].Position.Z;

            faceInfoPtr->Depth = depth / vertexCount;
            vertexAttribsPtr += vertexCount;
        }

        faceInfoPtr->VerticesStartIndex = verticesStartIndex;
        verticesStartIndex += vertexCount;

        faceInfoPtr++;
    }

    // Sort face infos by depth
    if (sortFaces)
        qsort(vertexBuffer->FaceInfoBuffer, vertexBuffer->FaceCount, sizeof(FaceInfo), SortPolygonFaces);

    // sas
    VertexAttribute *vertex, *vertexFirst;
    faceInfoPtr = vertexBuffer->FaceInfoBuffer;

    Texture *texturePtr;

    int widthSubpx = (int)(currentView->Width) << 16;
    int heightSubpx = (int)(currentView->Height) << 16;
    int widthHalfSubpx = widthSubpx >> 1;
    int heightHalfSubpx = heightSubpx >> 1;

#define PROJECT_X(pointX) ((pointX * currentView->Width * 0x10000) / vertexZ) - (cx << 16) + widthHalfSubpx
#define PROJECT_Y(pointY) ((pointY * currentView->Height * 0x10000) / vertexZ) - (cy << 16) + heightHalfSubpx

#define ORTHO_X(pointX) pointX - (cx << 16) + widthHalfSubpx
#define ORTHO_Y(pointY) pointY - (cy << 16) + heightHalfSubpx

    switch (drawMode & DrawMode_FillTypeMask) {
        // Lines, Solid Colored
        case DrawMode_LINES:
            for (int f = 0; f < vertexBuffer->FaceCount; f++) {
                vertexCountPerFaceMinus1 = faceInfoPtr->NumVertices - 1;
                vertexFirst = &vertexBuffer->Vertices[faceInfoPtr->VerticesStartIndex];
                vertex = vertexFirst;

                SET_BLENDFLAG_AND_OPACITY(faceInfoPtr);
                Alpha = opacity;
                BlendFlag = blendFlag;

                if (usePerspective) {
                    #define LINE_X(pos) ((float)PROJECT_X(pos)) / 0x10000
                    #define LINE_Y(pos) ((float)PROJECT_Y(pos)) / 0x10000
                    while (vertexCountPerFaceMinus1--) {
                        int vertexZ = vertex->Position.Z;
                        if (vertexZ < 0x10000)
                            goto mrt_line_solid_NEXT_FACE;

                        ColRGB = vertex->Color;
                        SoftwareRenderer::StrokeLine(LINE_X(vertex[0].Position.X), LINE_Y(vertex[0].Position.Y), LINE_X(vertex[1].Position.X), LINE_Y(vertex[1].Position.Y));
                        vertex++;
                    }
                    int vertexZ = vertex->Position.Z;
                    if (vertexZ < 0x10000)
                        goto mrt_line_solid_NEXT_FACE;
                    ColRGB = vertex->Color;
                    SoftwareRenderer::StrokeLine(LINE_X(vertex->Position.X), LINE_Y(vertex->Position.Y), LINE_X(vertexFirst->Position.X), LINE_Y(vertexFirst->Position.Y));
                }
                else {
                    #define LINE_ORTHO_X(pos) ((float)ORTHO_X(pos)) / 0x10000
                    #define LINE_ORTHO_Y(pos) ((float)ORTHO_Y(pos)) / 0x10000
                    while (vertexCountPerFaceMinus1--) {
                        ColRGB = vertex->Color;
                        SoftwareRenderer::StrokeLine(LINE_ORTHO_X(vertex[0].Position.X), LINE_ORTHO_Y(vertex[0].Position.Y), LINE_ORTHO_X(vertex[1].Position.X), LINE_ORTHO_Y(vertex[1].Position.Y));
                        vertex++;
                    }
                    ColRGB = vertex->Color;
                    SoftwareRenderer::StrokeLine(LINE_ORTHO_X(vertex->Position.X), LINE_ORTHO_Y(vertex->Position.Y), LINE_ORTHO_X(vertexFirst->Position.X), LINE_ORTHO_Y(vertexFirst->Position.Y));
                }

                mrt_line_solid_NEXT_FACE:
                faceInfoPtr++;
            }
            break;
        // Lines, Flat Shading
        case DrawMode_LINES_FLAT:
        // Lines, Smooth Shading
        case DrawMode_LINES_SMOOTH:
            for (int f = 0; f < vertexBuffer->FaceCount; f++) {
                vertexCount = faceInfoPtr->NumVertices;
                vertexCountPerFaceMinus1 = vertexCount - 1;
                vertexFirst = &vertexBuffer->Vertices[faceInfoPtr->VerticesStartIndex];
                vertex = vertexFirst;

                int averageNormalY = vertex[0].Normal.Y;
                for (int i = 1; i < vertexCount; i++)
                    averageNormalY += vertex[i].Normal.Y;
                averageNormalY /= vertexCount;

                ColRGB = CalcVertexColor(arrayBuffer, vertex, averageNormalY >> 8);
                SET_BLENDFLAG_AND_OPACITY(faceInfoPtr);
                Alpha = opacity;
                BlendFlag = blendFlag;

                if (usePerspective) {
                    while (vertexCountPerFaceMinus1--) {
                        int vertexZ = vertex->Position.Z;
                        if (vertexZ < 0x10000)
                            goto mrt_line_smooth_NEXT_FACE;

                        SoftwareRenderer::StrokeLine(LINE_X(vertex[0].Position.X), LINE_Y(vertex[0].Position.Y), LINE_X(vertex[1].Position.X), LINE_Y(vertex[1].Position.Y));
                        vertex++;
                    }
                    int vertexZ = vertex->Position.Z;
                    if (vertexZ < 0x10000)
                        goto mrt_line_smooth_NEXT_FACE;
                    SoftwareRenderer::StrokeLine(LINE_X(vertex->Position.X), LINE_Y(vertex->Position.Y), LINE_X(vertexFirst->Position.X), LINE_Y(vertexFirst->Position.Y));
                    #undef LINE_X
                    #undef LINE_Y
                }
                else {
                    while (vertexCountPerFaceMinus1--) {
                        SoftwareRenderer::StrokeLine(LINE_ORTHO_X(vertex[0].Position.X), LINE_ORTHO_Y(vertex[0].Position.Y), LINE_ORTHO_X(vertex[1].Position.X), LINE_ORTHO_Y(vertex[1].Position.Y));
                        vertex++;
                    }
                    SoftwareRenderer::StrokeLine(LINE_ORTHO_X(vertex->Position.X), LINE_ORTHO_Y(vertex->Position.Y), LINE_ORTHO_X(vertexFirst->Position.X), LINE_ORTHO_Y(vertexFirst->Position.Y));
                    #undef LINE_ORTHO_X
                    #undef LINE_ORTHO_Y
                }

                mrt_line_smooth_NEXT_FACE:
                faceInfoPtr++;
            }
            break;
        // Polygons, Solid Colored
        case DrawMode_POLYGONS:
            for (int f = 0; f < vertexBuffer->FaceCount; f++) {
                vertexCount = vertexCountPerFace = faceInfoPtr->NumVertices;
                vertexFirst = &vertexBuffer->Vertices[faceInfoPtr->VerticesStartIndex];
                vertex = vertexFirst;

                SET_BLENDFLAG_AND_OPACITY(faceInfoPtr);

                Vector3 polygonVertex[MAX_POLYGON_VERTICES];
                Vector2 polygonUV[MAX_POLYGON_VERTICES];
                int     polygonVertexIndex = 0;
                int     numOutside = 0;
                while (vertexCountPerFace--) {
                    int vertexZ = vertex->Position.Z;
                    if (usePerspective) {
                        if (vertexZ < 0x10000)
                            goto mrt_poly_solid_NEXT_FACE;

                        polygonVertex[polygonVertexIndex].X = PROJECT_X(vertex->Position.X);
                        polygonVertex[polygonVertexIndex].Y = PROJECT_Y(vertex->Position.Y);
                    }
                    else {
                        polygonVertex[polygonVertexIndex].X = ORTHO_X(vertex->Position.X);
                        polygonVertex[polygonVertexIndex].Y = ORTHO_Y(vertex->Position.Y);
                    }

#define POINT_IS_OUTSIDE(i) \
    (polygonVertex[i].X < 0 || polygonVertex[i].Y < 0 || polygonVertex[i].X > widthSubpx || polygonVertex[i].Y > heightSubpx)

                    if (POINT_IS_OUTSIDE(polygonVertexIndex))
                        numOutside++;

                    polygonVertex[polygonVertexIndex].Z = vertexZ;
                    polygonUV[polygonVertexIndex].X = vertex->UV.X;
                    polygonUV[polygonVertexIndex].Y = vertex->UV.Y;
                    polygonVertexIndex++;
                    vertex++;
                }

                if (numOutside == vertexCount)
                    goto mrt_poly_solid_NEXT_FACE;

                #define CHECK_TEXTURE(face) \
                    if (face->UseMaterial) \
                        texturePtr = (Texture*)face->Material.Texture

                texturePtr = NULL;
                if (drawMode & DrawMode_TEXTURED) {
                    CHECK_TEXTURE(faceInfoPtr);
                }

                if (texturePtr) {
                    if (!doAffineMapping)
                        DrawPolygonPerspective(texturePtr, polygonVertex, polygonUV, vertexFirst->Color, vertexCount, opacity, blendFlag);
                    else
                        DrawPolygonAffine(texturePtr, polygonVertex, polygonUV, vertexFirst->Color, vertexCount, opacity, blendFlag);
                }
                else {
                    if (UseDepthBuffer)
                        DrawPolygonDepth(polygonVertex, vertexFirst->Color, vertexCount, opacity, blendFlag);
                    else
                        DrawPolygonShaded(polygonVertex, vertexFirst->Color, vertexCount, opacity, blendFlag);
                }

                mrt_poly_solid_NEXT_FACE:
                faceInfoPtr++;
            }
            break;
        // Polygons, Flat Shading
        case DrawMode_POLYGONS_FLAT:
            for (int f = 0; f < vertexBuffer->FaceCount; f++) {
                vertexCount = vertexCountPerFace = faceInfoPtr->NumVertices;
                vertexFirst = &vertexBuffer->Vertices[faceInfoPtr->VerticesStartIndex];
                vertex = vertexFirst;

                int averageNormalY = vertex[0].Normal.Y;
                for (int i = 1; i < vertexCount; i++)
                    averageNormalY += vertex[i].Normal.Y;
                averageNormalY /= vertexCount;

                int color = CalcVertexColor(arrayBuffer, vertex, averageNormalY >> 8);
                SET_BLENDFLAG_AND_OPACITY(faceInfoPtr);

                Vector3 polygonVertex[MAX_POLYGON_VERTICES];
                Vector2 polygonUV[MAX_POLYGON_VERTICES];
                int     polygonVertexIndex = 0;
                int     numOutside = 0;
                while (vertexCountPerFace--) {
                    int vertexZ = vertex->Position.Z;
                    if (usePerspective) {
                        if (vertexZ < 0x10000)
                            goto mrt_poly_flat_NEXT_FACE;

                        polygonVertex[polygonVertexIndex].X = PROJECT_X(vertex->Position.X);
                        polygonVertex[polygonVertexIndex].Y = PROJECT_Y(vertex->Position.Y);
                    }
                    else {
                        polygonVertex[polygonVertexIndex].X = ORTHO_X(vertex->Position.X);
                        polygonVertex[polygonVertexIndex].Y = ORTHO_Y(vertex->Position.Y);
                    }

                    if (POINT_IS_OUTSIDE(polygonVertexIndex))
                        numOutside++;

                    polygonVertex[polygonVertexIndex].Z = vertexZ;
                    polygonUV[polygonVertexIndex].X = vertex->UV.X;
                    polygonUV[polygonVertexIndex].Y = vertex->UV.Y;
                    polygonVertexIndex++;
                    vertex++;
                }

                if (numOutside == vertexCount)
                    goto mrt_poly_flat_NEXT_FACE;

                texturePtr = NULL;
                if (drawMode & DrawMode_TEXTURED) {
                    CHECK_TEXTURE(faceInfoPtr);
                }

                if (texturePtr) {
                    if (!doAffineMapping)
                        DrawPolygonPerspective(texturePtr, polygonVertex, polygonUV, color, vertexCount, opacity, blendFlag);
                    else
                        DrawPolygonAffine(texturePtr, polygonVertex, polygonUV, color, vertexCount, opacity, blendFlag);
                }
                else {
                    if (UseDepthBuffer)
                        DrawPolygonDepth(polygonVertex, color, vertexCount, opacity, blendFlag);
                    else
                        DrawPolygonShaded(polygonVertex, color, vertexCount, opacity, blendFlag);
                }

                mrt_poly_flat_NEXT_FACE:
                faceInfoPtr++;
            }
            break;
        // Polygons, Smooth Shading
        case DrawMode_POLYGONS_SMOOTH:
            for (int f = 0; f < vertexBuffer->FaceCount; f++) {
                vertexCount = vertexCountPerFace = faceInfoPtr->NumVertices;
                vertexFirst = &vertexBuffer->Vertices[faceInfoPtr->VerticesStartIndex];
                vertex = vertexFirst;

                SET_BLENDFLAG_AND_OPACITY(faceInfoPtr);

#undef SET_BLENDFLAG_AND_OPACITY

                Vector3 polygonVertex[MAX_POLYGON_VERTICES];
                Vector2 polygonUV[MAX_POLYGON_VERTICES];
                int     polygonVertColor[MAX_POLYGON_VERTICES];
                int     polygonVertexIndex = 0;
                int     numOutside = 0;
                while (vertexCountPerFace--) {
                    int vertexZ = vertex->Position.Z;
                    if (usePerspective) {
                        if (vertexZ < 0x10000)
                            goto mrt_poly_smooth_NEXT_FACE;

                        polygonVertex[polygonVertexIndex].X = PROJECT_X(vertex->Position.X);
                        polygonVertex[polygonVertexIndex].Y = PROJECT_Y(vertex->Position.Y);
                    }
                    else {
                        polygonVertex[polygonVertexIndex].X = ORTHO_X(vertex->Position.X);
                        polygonVertex[polygonVertexIndex].Y = ORTHO_Y(vertex->Position.Y);
                    }

                    if (POINT_IS_OUTSIDE(polygonVertexIndex))
                        numOutside++;

                    polygonVertex[polygonVertexIndex].Z = vertexZ;

                    polygonUV[polygonVertexIndex].X = vertex->UV.X;
                    polygonUV[polygonVertexIndex].Y = vertex->UV.Y;

                    polygonVertColor[polygonVertexIndex] = CalcVertexColor(arrayBuffer, vertex, vertex->Normal.Y >> 8);
                    polygonVertexIndex++;
                    vertex++;
                }

                if (numOutside == vertexCount)
                    goto mrt_poly_smooth_NEXT_FACE;

#undef POINT_IS_OUTSIDE

                texturePtr = NULL;
                if (drawMode & DrawMode_TEXTURED) {
                    CHECK_TEXTURE(faceInfoPtr);
                }

                if (texturePtr) {
                    if (!doAffineMapping)
                        DrawPolygonBlendPerspective(texturePtr, polygonVertex, polygonUV, polygonVertColor, vertexCount, opacity, blendFlag);
                    else
                        DrawPolygonBlendAffine(texturePtr, polygonVertex, polygonUV, polygonVertColor, vertexCount, opacity, blendFlag);
                }
                else {
                    if (UseDepthBuffer)
                        DrawPolygonBlendDepth(polygonVertex, polygonVertColor, vertexCount, opacity, blendFlag);
                    else
                        DrawPolygonBlendShaded(polygonVertex, polygonVertColor, vertexCount, opacity, blendFlag);
                }

#undef CHECK_TEXTURE

                mrt_poly_smooth_NEXT_FACE:
                faceInfoPtr++;
            }
            break;
    }

#undef PROJECT_X
#undef PROJECT_Y

#undef ORTHO_X
#undef ORTHO_Y

    SoftwareRenderer::SetDepthTest(false);
}

PUBLIC STATIC Uint32   SoftwareRenderer::VertexBuffer_Create(Uint32 maxVertices, int unloadPolicy) {
    for (Uint32 i = 0; i < MAX_VERTEX_BUFFERS; i++) {
        if (VertexBuffers[i] == NULL) {
            VertexBuffers[i] = VertexBuffer::Create(maxVertices, unloadPolicy);
            return i;
        }
    }

    return 0xFFFFFFFF;
}
PUBLIC STATIC void     SoftwareRenderer::VertexBuffer_Bind(Uint32 vertexBufferIndex) {
    if (vertexBufferIndex < 0 || vertexBufferIndex >= MAX_VERTEX_BUFFERS)
        vertexBufferIndex = -1;
    CurrentVertexBuffer = vertexBufferIndex;
}
PUBLIC STATIC void     SoftwareRenderer::VertexBuffer_Delete(Uint32 vertexBufferIndex) {
    if (vertexBufferIndex < 0 || vertexBufferIndex >= MAX_ARRAY_BUFFERS)
        return;

    VertexBuffer::Delete(VertexBuffers[vertexBufferIndex]);
    VertexBuffers[vertexBufferIndex] = NULL;
}

static void SetFaceInfoMaterial(FaceInfo* face, Material* material) {
    face->UseMaterial = true;
    face->Material.Texture = NULL;

    Image *image = material->ImagePtr;
    if (image && image->TexturePtr)
        face->Material.Texture = (Texture*)image->TexturePtr;

    for (unsigned i = 0; i < 4; i++) {
        face->Material.Specular[i] = material->Specular[i] * 0x100;
        face->Material.Ambient[i] = material->Ambient[i] * 0x100;
        face->Material.Diffuse[i] = material->Diffuse[i] * 0x100;
    }
}
static void SetFaceInfoMaterial(FaceInfo* face, Texture* texture) {
    face->UseMaterial = true;
    face->Material.Texture = texture;

    for (unsigned i = 0; i < 4; i++) {
        face->Material.Specular[i] = 0x100;
        face->Material.Ambient[i] = 0x100;
        face->Material.Diffuse[i] = 0x100;
    }
}

#define APPLY_MAT4X4(vec4out, vec3in, M) { \
    float vecX = vec3in.X; \
    float vecY = vec3in.Y; \
    float vecZ = vec3in.Z; \
    vec4out.X = (M[ 3] * 0x10000) + ((int)(vecX * M[ 0])) + ((int)(vecY * M[ 1])) + ((int)(vecZ * M[ 2])); \
    vec4out.Y = (M[ 7] * 0x10000) + ((int)(vecX * M[ 4])) + ((int)(vecY * M[ 5])) + ((int)(vecZ * M[ 6])); \
    vec4out.Z = (M[11] * 0x10000) + ((int)(vecX * M[ 8])) + ((int)(vecY * M[ 9])) + ((int)(vecZ * M[10])); \
    vec4out.W = (M[15] * 0x10000) + ((int)(vecX * M[12])) + ((int)(vecY * M[13])) + ((int)(vecZ * M[14])); \
}
#define COPY_VECTOR(vecout, vecin) vecout = vecin
#define COPY_NORMAL(vec4out, vec3in) \
    vec4out.X = vec3in.X; \
    vec4out.Y = vec3in.Y; \
    vec4out.Z = vec3in.Z; \
    vec4out.W = 0

static void CalculateMVPMatrix(Matrix4x4* output, Matrix4x4* modelMatrix, Matrix4x4* viewMatrix, Matrix4x4* projMatrix) {
    if (viewMatrix && projMatrix) {
        Matrix4x4 modelView;

        if (modelMatrix) {
            Matrix4x4::Multiply(&modelView, modelMatrix, viewMatrix);
            Matrix4x4::Multiply(output, &modelView, projMatrix);
        }
        else
            Matrix4x4::Multiply(output, viewMatrix, projMatrix);
    }
    else if (modelMatrix)
        *output = *modelMatrix;
    else
        Matrix4x4::Identity(output);
}
static bool CheckPolygonVisible(VertexAttribute* vertex, int vertexCount) {
    int numBehind[3] = { 0, 0, 0 };
    int numVertices = vertexCount;
    while (numVertices--) {
        if (vertex->Position.X < -vertex->Position.W || vertex->Position.X > vertex->Position.W)
            numBehind[0]++;
        if (vertex->Position.Y < -vertex->Position.W || vertex->Position.Y > vertex->Position.W)
            numBehind[1]++;
        if (vertex->Position.Z <= 0)
            numBehind[2]++;

        vertex++;
    }

    if (numBehind[0] == vertexCount || numBehind[1] == vertexCount || numBehind[2] == vertexCount)
        return false;

    return true;
}
static int ClipPolygon(VertexBuffer* vertexBuffer, VertexAttribute* output, VertexAttribute* input, int numVertices) {
    PolygonClipBuffer clipper;
    clipper.NumPoints = 0;
    clipper.MaxPoints = MAX_POLYGON_VERTICES;

    int numOutVertices = Clipper::FrustumClip(&clipper, ViewFrustum, NumFrustumPlanes, input, numVertices);
    if (numOutVertices < 3 || numOutVertices >= MAX_POLYGON_VERTICES)
        return 0;
    else if (vertexBuffer->VertexCount + numOutVertices > vertexBuffer->Capacity)
        return -1;

    VertexAttribute* result = clipper.Buffer;
    int n = numOutVertices;
    while (n--) {
        COPY_VECTOR(output->Position, result->Position);
        COPY_NORMAL(output->Normal, result->Normal);
        output->Color = result->Color;
        output->UV = result->UV;
        output++;
        result++;
    }

    return numOutVertices;
}

#define GET_ARRAY_OR_VERTEX_BUFFER(arrayBufferIndex, vertexBufferIndex) \
    if (vertexBufferIndex != -1) { \
        vertexBuffer = VertexBuffers[vertexBufferIndex]; \
        if (!vertexBuffer) \
            return; \
    } \
    else { \
        if (arrayBufferIndex == -1) \
            return; \
        arrayBuffer = &ArrayBuffers[arrayBufferIndex]; \
        vertexBuffer = &arrayBuffer->Buffer; \
        if (!arrayBuffer->Initialized) \
            return; \
    }

PUBLIC STATIC void     SoftwareRenderer::DrawPolygon3D(VertexAttribute* data, int vertexCount, int vertexFlag, Texture* texture, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {
    ArrayBuffer* arrayBuffer = NULL;
    VertexBuffer* vertexBuffer = NULL;

    GET_ARRAY_OR_VERTEX_BUFFER(CurrentArrayBuffer, CurrentVertexBuffer);

    Matrix4x4 mvpMatrix;
    if (arrayBuffer)
        CalculateMVPMatrix(&mvpMatrix, modelMatrix, &arrayBuffer->ViewMatrix, &arrayBuffer->ProjectionMatrix);
    else
        CalculateMVPMatrix(&mvpMatrix, modelMatrix, NULL, NULL);

    int arrayFaceCount = vertexBuffer->FaceCount;
    int arrayVertexCount = vertexBuffer->VertexCount;

    FaceInfo* faceInfoItem = &vertexBuffer->FaceInfoBuffer[arrayFaceCount];
    VertexAttribute* arrayVertexBuffer = &vertexBuffer->Vertices[arrayVertexCount];

    if (arrayVertexCount + vertexCount > vertexBuffer->Capacity) {
        BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Vertex buffer of size %d is full! Increase its size by %d!", vertexBuffer->Capacity, (arrayVertexCount + vertexCount) - vertexBuffer->Capacity);
        return;
    }

    VertexAttribute* vertex = arrayVertexBuffer;
    int numVertices = vertexCount;
    int numBehind = 0;
    while (numVertices--) {
        // Calculate position
        APPLY_MAT4X4(vertex->Position, data->Position, mvpMatrix.Values);

        // Calculate normals
        if (vertexFlag & VertexType_Normal) {
            if (normalMatrix) {
                APPLY_MAT4X4(vertex->Normal, data->Normal, normalMatrix->Values);
            }
            else {
                COPY_NORMAL(vertex->Normal, data->Normal);
            }
        }
        else
            vertex->Normal.X = vertex->Normal.Y = vertex->Normal.Z = vertex->Normal.W = 0;

        if (vertexFlag & VertexType_Color)
            vertex->Color = ColorMultiply(data->Color, ColRGB);
        else
            vertex->Color = ColRGB;

        if (vertexFlag & VertexType_UV)
            vertex->UV = data->UV;

        if (arrayBuffer && vertex->Position.Z <= 0x10000)
            numBehind++;

        vertex++;
        data++;
    }

    // Don't clip polygons if drawing into a vertex buffer, since the vertices are not in clip space
    if (arrayBuffer) {
        // Check if the polygon is at least partially inside the frustum
        if (!CheckPolygonVisible(arrayVertexBuffer, vertexCount))
            return;

        // Vertices are now in clip space, which means that the polygon can be frustum clipped
        if (ClipPolygonsByFrustum) {
            vertexCount = ClipPolygon(vertexBuffer, arrayVertexBuffer, arrayVertexBuffer, vertexCount);
            if (vertexCount == -1) {
                BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Vertex buffer of size %d is full!", vertexBuffer->Capacity);
                return;
            }
            else if (vertexCount == 0)
                return;
        } else if (numBehind != 0)
            return;
    }

    faceInfoItem->NumVertices = vertexCount;
    if (texture)
        SetFaceInfoMaterial(faceInfoItem, texture);
    else
        faceInfoItem->UseMaterial = false;
    if (arrayBuffer) {
        faceInfoItem->Opacity = Alpha;
        faceInfoItem->BlendFlag = BlendFlag;
    }

    vertexBuffer->VertexCount += vertexCount;
    vertexBuffer->FaceCount++;
}
PUBLIC STATIC void     SoftwareRenderer::DrawSceneLayer3D(SceneLayer* layer, int sx, int sy, int sw, int sh, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {
    int vertexCountPerFace = 4;
    int tileSize = 16;

    ArrayBuffer* arrayBuffer = NULL;
    VertexBuffer* vertexBuffer = NULL;

    GET_ARRAY_OR_VERTEX_BUFFER(CurrentArrayBuffer, CurrentVertexBuffer);

    Matrix4x4 mvpMatrix;
    if (arrayBuffer)
        CalculateMVPMatrix(&mvpMatrix, modelMatrix, &arrayBuffer->ViewMatrix, &arrayBuffer->ProjectionMatrix);
    else
        CalculateMVPMatrix(&mvpMatrix, modelMatrix, NULL, NULL);

    int arrayVertexCount = vertexBuffer->VertexCount;
    int arrayFaceCount = vertexBuffer->FaceCount;
    FaceInfo* faceInfoItem = &vertexBuffer->FaceInfoBuffer[arrayFaceCount];
    VertexAttribute* arrayVertexBuffer = &vertexBuffer->Vertices[arrayVertexCount];

    vector<AnimFrame> animFrames;
    vector<Texture*> textureSources;
    animFrames.resize(Scene::TileSpriteInfos.size());
    textureSources.resize(Scene::TileSpriteInfos.size());
    for (size_t i = 0; i < Scene::TileSpriteInfos.size(); i++) {
        TileSpriteInfo info = Scene::TileSpriteInfos[i];
        animFrames[i] = info.Sprite->Animations[info.AnimationIndex].Frames[info.FrameIndex];
        textureSources[i] = info.Sprite->Spritesheets[animFrames[i].SheetNumber];
    }

    int totalVertexCount = 0;
    for (int y = sy; y < sh; y++) {
        for (int x = sx; x < sw; x++) {
            int tileID = (int)(layer->Tiles[x + (y << layer->WidthInBits)] & TILE_IDENT_MASK);
            if (tileID != Scene::EmptyTile && tileID < Scene::TileSpriteInfos.size())
                totalVertexCount += vertexCountPerFace;
        }
    }

    if (arrayVertexCount + totalVertexCount > vertexBuffer->Capacity) {
        BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Vertex buffer of size %d is full! Increase its size by %d!", vertexBuffer->Capacity, (arrayVertexCount + totalVertexCount) - vertexBuffer->Capacity);
        return;
    }

    for (int y = sy, destY = 0; y < sh; y++, destY++) {
        for (int x = sx, destX = 0; x < sw; x++, destX++) {
            int tileAtPos = layer->Tiles[x + (y << layer->WidthInBits)];
            int tileID = tileAtPos & TILE_IDENT_MASK;
            if (tileID == Scene::EmptyTile || tileID >= Scene::TileSpriteInfos.size())
                continue;

            // 0--1
            // |  |
            // 3--2
            VertexAttribute data[4];
            AnimFrame frameStr = animFrames[tileID];
            Texture* texture = textureSources[tileID];

            float uv_left   = (float)frameStr.X / texture->Width;
            float uv_right  = (float)(frameStr.X + frameStr.Width) / texture->Width;
            float uv_top    = (float)frameStr.Y / texture->Height;
            float uv_bottom = (float)(frameStr.Y + frameStr.Height) / texture->Height;

            float left_u, right_u, top_v, bottom_v;
            int flipX = tileAtPos & TILE_FLIPX_MASK;
            int flipY = tileAtPos & TILE_FLIPY_MASK;

            if (flipX) {
                left_u  = uv_right;
                right_u = uv_left;
            } else {
                left_u  = uv_left;
                right_u = uv_right;
            }

            if (flipY) {
                top_v    = uv_bottom;
                bottom_v = uv_top;
            } else {
                top_v    = uv_top;
                bottom_v = uv_bottom;
            }

            data[0].Position.X = (destX * tileSize) * 0x10000;
            data[0].Position.Z = (destY * tileSize) * 0x10000;
            data[0].Position.Y = 0;
            data[0].UV.X       = left_u * 0x10000;
            data[0].UV.Y       = top_v * 0x10000;
            data[0].Normal.X   = data[0].Normal.Y = data[0].Normal.Z = data[0].Normal.W = 0;

            data[1].Position.X = data[0].Position.X + (tileSize * 0x10000);
            data[1].Position.Z = data[0].Position.Z;
            data[1].Position.Y = 0;
            data[1].UV.X       = right_u * 0x10000;
            data[1].UV.Y       = top_v * 0x10000;
            data[1].Normal.X   = data[1].Normal.Y = data[1].Normal.Z = data[1].Normal.W = 0;

            data[2].Position.X = data[1].Position.X;
            data[2].Position.Z = data[1].Position.Z + (tileSize * 0x10000);
            data[2].Position.Y = 0;
            data[2].UV.X       = right_u * 0x10000;
            data[2].UV.Y       = bottom_v * 0x10000;
            data[2].Normal.X   = data[2].Normal.Y = data[2].Normal.Z = data[2].Normal.W = 0;

            data[3].Position.X = data[0].Position.X;
            data[3].Position.Z = data[2].Position.Z;
            data[3].Position.Y = 0;
            data[3].UV.X       = left_u * 0x10000;
            data[3].UV.Y       = bottom_v * 0x10000;
            data[3].Normal.X   = data[3].Normal.Y = data[3].Normal.Z = data[3].Normal.W = 0;

            VertexAttribute* vertex = arrayVertexBuffer;
            int vertexIndex = 0;
            while (vertexIndex < vertexCountPerFace) {
                // Calculate position
                APPLY_MAT4X4(vertex->Position, data[vertexIndex].Position, mvpMatrix.Values);

                // Calculate normals
                if (normalMatrix) {
                    APPLY_MAT4X4(vertex->Normal, data[vertexIndex].Normal, normalMatrix->Values);
                }
                else {
                    COPY_NORMAL(vertex->Normal, data[vertexIndex].Normal);
                }

                vertex->UV = data[vertexIndex].UV;
                vertex->Color = ColRGB;

                vertex++;
                vertexIndex++;
            }

            int vertexCount = vertexCountPerFace;
            if (arrayBuffer) {
                // Check if the polygon is at least partially inside the frustum
                if (!CheckPolygonVisible(arrayVertexBuffer, vertexCount))
                    continue;

                // Vertices are now in clip space, which means that the polygon can be frustum clipped
                if (ClipPolygonsByFrustum) {
                    vertexCount = ClipPolygon(vertexBuffer, arrayVertexBuffer, arrayVertexBuffer, vertexCount);
                    if (vertexCount == -1) {
                        BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Vertex buffer of size %d is full!", vertexBuffer->Capacity);
                        return;
                    }
                    else if (vertexCount == 0)
                        continue;
                }
            }

            if (texture)
                SetFaceInfoMaterial(faceInfoItem, texture);
            else
                faceInfoItem->UseMaterial = false;
            faceInfoItem->Opacity = Alpha;
            faceInfoItem->BlendFlag = BlendFlag;
            faceInfoItem->NumVertices = vertexCount;
            faceInfoItem++;
            arrayVertexCount += vertexCount;
            arrayVertexBuffer += vertexCount;
            arrayFaceCount++;
        }
    }

    vertexBuffer->VertexCount = arrayVertexCount;
    vertexBuffer->FaceCount = arrayFaceCount;
}

class ModelRenderer {
public:
    ModelRenderer(VertexBuffer* vertexBuffer);

    void DrawModel(IModel* model, Uint32 frame);
    void DrawModel(IModel* model, Uint16 animation, Uint32 frame);
    void DrawMesh(IModel* model, Mesh* mesh, Skeleton* skeleton, Matrix4x4& mvpMatrix);
    void DrawNode(IModel* model, ModelNode* node, Matrix4x4* world);
    void AddFace(int faceVertexCount, Material* material);
    int  ClipFace(int faceVertexCount);
    void SetMatrices(Matrix4x4* model, Matrix4x4* view, Matrix4x4* projection, Matrix4x4* normal);

    VertexBuffer*    Buffer;
    int              VertexCount;

    VertexAttribute* AttribBuffer;
    VertexAttribute* Vertex;
    FaceInfo*        FaceItem;

    Matrix4x4*       ModelMatrix;
    Matrix4x4*       ViewMatrix;
    Matrix4x4*       ProjectionMatrix;
    Matrix4x4*       NormalMatrix;

    Matrix4x4        MVPMatrix;

    bool             ClipFaces;
    Armature*        ArmaturePtr;
};

ModelRenderer::ModelRenderer(VertexBuffer* vertexBuffer) {
    Buffer = vertexBuffer;
    VertexCount = Buffer->VertexCount;

    FaceItem = &Buffer->FaceInfoBuffer[Buffer->FaceCount];
    AttribBuffer = Vertex = &Buffer->Vertices[VertexCount];

    ClipFaces = false;
    ArmaturePtr = nullptr;
}

void ModelRenderer::SetMatrices(Matrix4x4* model, Matrix4x4* view, Matrix4x4* projection, Matrix4x4* normal) {
    ModelMatrix = model;
    ViewMatrix = view;
    ProjectionMatrix = projection;
    NormalMatrix = normal;
}

void ModelRenderer::AddFace(int faceVertexCount, Material* material) {
    FaceItem->NumVertices = faceVertexCount;
    FaceItem->Opacity = Alpha;
    FaceItem->BlendFlag = BlendFlag;

    if (material)
        SetFaceInfoMaterial(FaceItem, material);
    else
        FaceItem->UseMaterial = false;

    FaceItem++;

    Buffer->FaceCount++;
    Buffer->VertexCount += faceVertexCount;
}

int ModelRenderer::ClipFace(int faceVertexCount) {
    if (!ClipFaces)
        return faceVertexCount;

    Vertex = AttribBuffer;

    if (!CheckPolygonVisible(Vertex, faceVertexCount))
        return 0;
    else if (ClipPolygonsByFrustum) {
        faceVertexCount = ClipPolygon(Buffer, AttribBuffer, Vertex, faceVertexCount);
        if (faceVertexCount == -1) {
            BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Vertex buffer of size %d is full!", Buffer->Capacity);
            throw -1;
        }
        else if (faceVertexCount == 0)
            return 0;
    }

    Vertex += faceVertexCount;
    AttribBuffer = Vertex;

    return faceVertexCount;
}

void ModelRenderer::DrawMesh(IModel* model, Mesh* mesh, Skeleton* skeleton, Matrix4x4& mvpMatrix) {
    Material* material = mesh->MaterialIndex != -1 ? model->Materials[mesh->MaterialIndex] : nullptr;

    int modelVertexIndexCount = mesh->VertexIndexCount;
    Sint16* modelVertexIndexPtr = mesh->VertexIndexBuffer;

    int vertexTypeMask = VertexType_Position | VertexType_Normal | VertexType_Color | VertexType_UV;
    int color = ColRGB;

    Vector3* positionPtr;
    Vector3* normalPtr;
    Uint32* colorPtr;
    Vector2* uvPtr;

    Vector3* positionBuffer = mesh->PositionBuffer;
    Vector3* normalBuffer = mesh->NormalBuffer;

    if (skeleton) {
        positionBuffer = skeleton->TransformedPositions;
        normalBuffer = skeleton->TransformedNormals;
    }

    switch (mesh->VertexFlag & vertexTypeMask) {
        case VertexType_Position:
            // For every face,
            while (*modelVertexIndexPtr != -1) {
                int faceVertexCount = model->FaceVertexCount;

                // For every vertex index,
                int numVertices = faceVertexCount;
                while (numVertices--) {
                    positionPtr = &positionBuffer[*modelVertexIndexPtr];
                    APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                    Vertex->Color = color;
                    modelVertexIndexPtr++;
                    Vertex++;
                }

                if (faceVertexCount = ClipFace(faceVertexCount))
                    AddFace(faceVertexCount, material);
            }
            break;
        case VertexType_Position | VertexType_Normal:
            if (NormalMatrix) {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->FaceVertexCount;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        // Calculate position
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        // Calculate normals
                        APPLY_MAT4X4(Vertex->Normal, normalPtr[0], NormalMatrix->Values);
                        Vertex->Color = color;
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if (faceVertexCount = ClipFace(faceVertexCount))
                        AddFace(faceVertexCount, material);
                }
            }
            else {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->FaceVertexCount;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        // Calculate position
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        COPY_NORMAL(Vertex->Normal, normalPtr[0]);
                        Vertex->Color = color;
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if (faceVertexCount = ClipFace(faceVertexCount))
                        AddFace(faceVertexCount, material);
                }
            }
            break;
        case VertexType_Position | VertexType_Normal | VertexType_Color:
            if (NormalMatrix) {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->FaceVertexCount;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        colorPtr = &mesh->ColorBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        APPLY_MAT4X4(Vertex->Normal, normalPtr[0], NormalMatrix->Values);
                        Vertex->Color = ColorMultiply(colorPtr[0], color);
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if (faceVertexCount = ClipFace(faceVertexCount))
                        AddFace(faceVertexCount, material);
                }
            }
            else {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->FaceVertexCount;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        colorPtr = &mesh->ColorBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        COPY_NORMAL(Vertex->Normal, normalPtr[0]);
                        Vertex->Color = ColorMultiply(colorPtr[0], color);
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if (faceVertexCount = ClipFace(faceVertexCount))
                        AddFace(faceVertexCount, material);
                }
            }
            break;
        case VertexType_Position | VertexType_Normal | VertexType_UV:
            if (NormalMatrix) {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->FaceVertexCount;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        uvPtr = &mesh->UVBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        APPLY_MAT4X4(Vertex->Normal, normalPtr[0], NormalMatrix->Values);
                        Vertex->Color = color;
                        Vertex->UV = uvPtr[0];
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if (faceVertexCount = ClipFace(faceVertexCount))
                        AddFace(faceVertexCount, material);
                }
            }
            else {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->FaceVertexCount;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        uvPtr = &mesh->UVBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        COPY_NORMAL(Vertex->Normal, normalPtr[0]);
                        Vertex->Color = color;
                        Vertex->UV = uvPtr[0];
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if (faceVertexCount = ClipFace(faceVertexCount))
                        AddFace(faceVertexCount, material);
                }
            }
            break;
        case VertexType_Position | VertexType_Normal | VertexType_UV | VertexType_Color:
            if (NormalMatrix) {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->FaceVertexCount;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        uvPtr = &mesh->UVBuffer[*modelVertexIndexPtr];
                        colorPtr = &mesh->ColorBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        APPLY_MAT4X4(Vertex->Normal, normalPtr[0], NormalMatrix->Values);
                        Vertex->Color = ColorMultiply(colorPtr[0], color);
                        Vertex->UV = uvPtr[0];
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if (faceVertexCount = ClipFace(faceVertexCount))
                        AddFace(faceVertexCount, material);
                }
            }
            else {
                // For every face,
                while (*modelVertexIndexPtr != -1) {
                    int faceVertexCount = model->FaceVertexCount;

                    // For every vertex index,
                    int numVertices = faceVertexCount;
                    while (numVertices--) {
                        positionPtr = &positionBuffer[*modelVertexIndexPtr];
                        normalPtr = &normalBuffer[*modelVertexIndexPtr];
                        uvPtr = &mesh->UVBuffer[*modelVertexIndexPtr];
                        colorPtr = &mesh->ColorBuffer[*modelVertexIndexPtr];
                        APPLY_MAT4X4(Vertex->Position, positionPtr[0], mvpMatrix.Values);
                        COPY_NORMAL(Vertex->Normal, normalPtr[0]);
                        Vertex->Color = ColorMultiply(colorPtr[0], color);
                        Vertex->UV = uvPtr[0];
                        modelVertexIndexPtr++;
                        Vertex++;
                    }

                    if (faceVertexCount = ClipFace(faceVertexCount))
                        AddFace(faceVertexCount, material);
                }
            }
            break;
    }
}

void ModelRenderer::DrawNode(IModel* model, ModelNode* node, Matrix4x4* world) {
    size_t numMeshes = node->Meshes.size();
    size_t numChildren = node->Children.size();

    Matrix4x4::Multiply(world, world, node->TransformMatrix);
    Matrix4x4 nodeToWorldMat;
    bool madeMatrix = false;

    for (size_t i = 0; i < numMeshes; i++) {
        Mesh* mesh = node->Meshes[i];

        if (mesh->SkeletonIndex != -1) // in world space
            DrawMesh(model, mesh, ArmaturePtr->Skeletons[mesh->SkeletonIndex], MVPMatrix);
        else {
            if (!madeMatrix) {
                if (ViewMatrix && ProjectionMatrix) {
                    Matrix4x4::Multiply(&nodeToWorldMat, world, ModelMatrix);
                    Matrix4x4::Multiply(&nodeToWorldMat, &nodeToWorldMat, ViewMatrix);
                    Matrix4x4::Multiply(&nodeToWorldMat, &nodeToWorldMat, ProjectionMatrix);
                } else
                    Matrix4x4::Multiply(&nodeToWorldMat, world, ModelMatrix);

                madeMatrix = true;
            }

            DrawMesh(model, mesh, nullptr, nodeToWorldMat);
        }
    }

    for (size_t i = 0; i < numChildren; i++)
        DrawNode(model, node->Children[i], world);
}

void ModelRenderer::DrawModel(IModel* model, Uint32 frame) {
    if (ViewMatrix && ProjectionMatrix)
        CalculateMVPMatrix(&MVPMatrix, ModelMatrix, ViewMatrix, ProjectionMatrix);
    else
        CalculateMVPMatrix(&MVPMatrix, ModelMatrix, NULL, NULL);

    if (!model->UseVertexAnimation) {
        Matrix4x4 identity;
        Matrix4x4::Identity(&identity);

        try {
            DrawNode(model, model->BaseArmature->RootNode, &identity);
        } catch (...) { }
    }
    else {
        static Skeleton vertexAnimSkeleton;

        frame = model->GetKeyFrame(frame) % model->FrameCount;

        // Just render every mesh directly
        for (int i = 0; i < model->MeshCount; i++) {
            Mesh* mesh = model->Meshes[i];

            Skeleton *skeletonPtr = nullptr;
            if (frame) {
                skeletonPtr = &vertexAnimSkeleton;
                skeletonPtr->TransformedPositions = mesh->PositionBuffer + (frame * model->VertexCount);
                skeletonPtr->TransformedNormals = mesh->NormalBuffer + (frame * model->VertexCount);
            }

            try {
                DrawMesh(model, mesh, skeletonPtr, MVPMatrix);
            } catch (...) {
                // Stop rendering meshes
                return;
            }
        }
    }
}

void ModelRenderer::DrawModel(IModel* model, Uint16 animation, Uint32 frame) {
    if (!model->UseVertexAnimation) {
        if (ArmaturePtr == nullptr)
            ArmaturePtr = model->BaseArmature;

        Uint16 numAnims = model->AnimationCount;
        if (numAnims > 0) {
            if (animation >= numAnims)
                animation = numAnims - 1;

            model->Animate(ArmaturePtr, model->Animations[animation], frame);
        }
    }

    DrawModel(model, frame);
}

PUBLIC STATIC void     SoftwareRenderer::DrawModel(IModel* model, Uint16 animation, Uint32 frame, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {
    ArrayBuffer* arrayBuffer = NULL;
    VertexBuffer* vertexBuffer = NULL;

    if (animation < 0 || frame < 0)
        return;
    else if (!model->UseVertexAnimation && animation >= model->AnimationCount)
        return;

    GET_ARRAY_OR_VERTEX_BUFFER(CurrentArrayBuffer, CurrentVertexBuffer);

    Matrix4x4* viewMatrix = nullptr;
    Matrix4x4* projMatrix = nullptr;

    if (arrayBuffer) {
        viewMatrix = &arrayBuffer->ViewMatrix;
        projMatrix = &arrayBuffer->ProjectionMatrix;
    }

    ModelRenderer rend = ModelRenderer(vertexBuffer);

    int modelVertexIndexCount = model->VertexIndexCount;
    int arrayVertexCount = rend.VertexCount;
    if (arrayVertexCount + modelVertexIndexCount > vertexBuffer->Capacity) {
        BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Model has too many vertices (%d) to fit in size (%d) of vertex buffer! Increase vertex buffer size by %d, or use a model with less vertices!", modelVertexIndexCount, vertexBuffer->Capacity, (arrayVertexCount + modelVertexIndexCount) - vertexBuffer->Capacity);
        return;
    }

    rend.ClipFaces = arrayBuffer != nullptr;
    rend.SetMatrices(modelMatrix, viewMatrix, projMatrix, normalMatrix);
    rend.DrawModel(model, animation, frame);
}
PUBLIC STATIC void     SoftwareRenderer::DrawModelSkinned(IModel* model, Uint16 armature, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {
    ArrayBuffer* arrayBuffer = NULL;
    VertexBuffer* vertexBuffer = NULL;

    if (model->UseVertexAnimation) {
        SoftwareRenderer::DrawModel(model, 0, 0, modelMatrix, normalMatrix);
        return;
    }

    if (armature >= model->ArmatureCount)
        return;

    GET_ARRAY_OR_VERTEX_BUFFER(CurrentArrayBuffer, CurrentVertexBuffer);

    Matrix4x4* viewMatrix = nullptr;
    Matrix4x4* projMatrix = nullptr;

    if (arrayBuffer) {
        viewMatrix = &arrayBuffer->ViewMatrix;
        projMatrix = &arrayBuffer->ProjectionMatrix;
    }

    ModelRenderer rend = ModelRenderer(vertexBuffer);

    int modelVertexIndexCount = model->VertexIndexCount;
    int arrayVertexCount = rend.VertexCount;
    if (arrayVertexCount + modelVertexIndexCount > vertexBuffer->Capacity) {
        BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Model has too many vertices (%d) to fit in size (%d) of vertex buffer! Increase vertex buffer size by %d, or use a model with less vertices!", modelVertexIndexCount, vertexBuffer->Capacity, (arrayVertexCount + modelVertexIndexCount) - vertexBuffer->Capacity);
        return;
    }

    rend.ClipFaces = arrayBuffer != nullptr;
    rend.ArmaturePtr = model->ArmatureList[armature];
    rend.SetMatrices(modelMatrix, viewMatrix, projMatrix, normalMatrix);
    rend.DrawModel(model, 0);
}
PUBLIC STATIC void     SoftwareRenderer::DrawVertexBuffer(Uint32 vertexBufferIndex, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix) {
    if (CurrentArrayBuffer == -1)
        return;

    ArrayBuffer* arrayBuffer = &ArrayBuffers[CurrentArrayBuffer];
    if (!arrayBuffer->Initialized)
        return;

    VertexBuffer* vertexBuffer = VertexBuffers[vertexBufferIndex];
    if (!vertexBuffer)
        return;

    Matrix4x4 mvpMatrix;
    CalculateMVPMatrix(&mvpMatrix, modelMatrix, &arrayBuffer->ViewMatrix, &arrayBuffer->ProjectionMatrix);

    // destination
    VertexBuffer* destVertexBuffer = &arrayBuffer->Buffer;
    int arrayFaceCount = destVertexBuffer->FaceCount;
    int arrayVertexCount = destVertexBuffer->VertexCount;

    FaceInfo* faceInfoItem = &destVertexBuffer->FaceInfoBuffer[arrayFaceCount];
    VertexAttribute* arrayVertexBuffer = &destVertexBuffer->Vertices[arrayVertexCount];
    VertexAttribute* arrayVertexItem = arrayVertexBuffer;

    // source
    int srcFaceCount = vertexBuffer->FaceCount;
    int srcVertexCount = vertexBuffer->VertexCount;
    if (!srcFaceCount || !srcVertexCount)
        return;

    if (arrayVertexCount + srcVertexCount > destVertexBuffer->Capacity) {
        BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Vertex buffer of size %d is full! Increase its size by %d!", destVertexBuffer->Capacity, (arrayVertexCount + srcVertexCount) - destVertexBuffer->Capacity);
        return;
    }

    // Copy the vertices into the vertex buffer
    VertexAttribute* srcVertexItem = &vertexBuffer->Vertices[0];

    for (int f = 0; f < srcFaceCount; f++) {
        FaceInfo* srcFaceInfoItem = &vertexBuffer->FaceInfoBuffer[f];
        int vertexCount = srcFaceInfoItem->NumVertices;
        int vertexCountPerFace = vertexCount;
        while (vertexCountPerFace--) {
            APPLY_MAT4X4(arrayVertexItem->Position, srcVertexItem->Position, mvpMatrix.Values);

            if (normalMatrix) {
                APPLY_MAT4X4(arrayVertexItem->Normal, srcVertexItem->Normal, normalMatrix->Values);
            }
            else {
                COPY_NORMAL(arrayVertexItem->Normal, srcVertexItem->Normal);
            }

            arrayVertexItem->Color = srcVertexItem->Color;
            arrayVertexItem->UV = srcVertexItem->UV;
            arrayVertexItem++;
            srcVertexItem++;
        }

        arrayVertexItem = arrayVertexBuffer;

        // Check if the polygon is at least partially inside the frustum
        if (!CheckPolygonVisible(arrayVertexBuffer, vertexCount))
            continue;

        // Vertices are now in clip space, which means that the polygon can be frustum clipped
        if (ClipPolygonsByFrustum) {
            vertexCount = ClipPolygon(vertexBuffer, arrayVertexBuffer, arrayVertexBuffer, vertexCount);
            if (vertexCount == -1) {
                BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Vertex buffer of size %d is full!", vertexBuffer->Capacity);
                return;
            }
            else if (vertexCount == 0)
                continue;
        }

        faceInfoItem->UseMaterial = srcFaceInfoItem->UseMaterial;
        if (faceInfoItem->UseMaterial)
            faceInfoItem->Material = srcFaceInfoItem->Material;
        faceInfoItem->NumVertices = vertexCount;
        faceInfoItem->Opacity = Alpha;
        faceInfoItem->BlendFlag = BlendFlag;
        faceInfoItem++;
        srcFaceInfoItem++;

        arrayVertexCount += vertexCount;
        arrayVertexBuffer += vertexCount;
        arrayVertexItem = arrayVertexBuffer;
        arrayFaceCount++;
    }

    destVertexBuffer->VertexCount = arrayVertexCount;
    destVertexBuffer->FaceCount = arrayFaceCount;
}

#undef APPLY_MAT4X4
#undef COPY_VECTOR
#undef COPY_NORMAL

#undef GET_ARRAY_OR_VERTEX_BUFFER

PUBLIC STATIC void     SoftwareRenderer::SetLineWidth(float n) {

}
PUBLIC STATIC void     SoftwareRenderer::StrokeLine(float x1, float y1, float x2, float y2) {
    int x = 0, y = 0;
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;

    View* currentView = &Scene::Views[Scene::ViewCurrent];
    int cx = (int)std::floor(currentView->X);
    int cy = (int)std::floor(currentView->Y);

    Matrix4x4* out = Graphics::ModelViewMatrix.top();
    x += out->Values[12];
    y += out->Values[13];
    x -= cx;
    y -= cy;

    int dst_x1 = x + x1;
    int dst_y1 = y + y1;
    int dst_x2 = x + x2;
    int dst_y2 = y + y2;

    int minX, maxX, minY, maxY;
    if (Graphics::CurrentClip.Enabled) {
        minX = Graphics::CurrentClip.X;
        minY = Graphics::CurrentClip.Y;
        maxX = Graphics::CurrentClip.X + Graphics::CurrentClip.Width;
        maxY = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
    }
    else {
        minX = 0;
        minY = 0;
        maxX = (int)Graphics::CurrentRenderTarget->Width;
        maxY = (int)Graphics::CurrentRenderTarget->Height;
    }

    int dx = Math::Abs(dst_x2 - dst_x1), sx = dst_x1 < dst_x2 ? 1 : -1;
    int dy = Math::Abs(dst_y2 - dst_y1), sy = dst_y1 < dst_y2 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    int blendFlag = BlendFlag;
    int opacity = Alpha;
    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Uint32 col = ColRGB;

    #define DRAW_LINE(pixelFunction) while (true) { \
        if (dst_x1 >= minX && dst_y1 >= minY && dst_x1 < maxX && dst_y1 < maxY) \
            pixelFunction((Uint32*)&col, &dstPx[dst_x1 + dst_y1 * dstStride], opacity, multTableAt, multSubTableAt); \
        if (dst_x1 == dst_x2 && dst_y1 == dst_y2) break; \
        e2 = err; \
        if (e2 > -dx) { err -= dy; dst_x1 += sx; } \
        if (e2 <  dy) { err += dx; dst_y1 += sy; } \
    }

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    // int dst_strideY = dst_y1 * dstStride;
    switch (blendFlag) {
        case 0:
            DRAW_LINE(PixelNoFiltSetOpaque);
            break;
        case 1:
            DRAW_LINE(PixelNoFiltSetTransparent);
            break;
        case 2:
            DRAW_LINE(PixelNoFiltSetAdditive);
            break;
        case 3:
            DRAW_LINE(PixelNoFiltSetSubtract);
            break;
        case 4:
            DRAW_LINE(PixelNoFiltSetMatchEqual);
            break;
        case 5:
            DRAW_LINE(PixelNoFiltSetMatchNotEqual);
            break;
        case 6:
            DRAW_LINE(PixelNoFiltSetFilter);
            break;
    }

    #undef DRAW_LINE
}
PUBLIC STATIC void     SoftwareRenderer::StrokeCircle(float x, float y, float rad) {

}
PUBLIC STATIC void     SoftwareRenderer::StrokeEllipse(float x, float y, float w, float h) {

}
PUBLIC STATIC void     SoftwareRenderer::StrokeRectangle(float x, float y, float w, float h) {

}

PUBLIC STATIC void     SoftwareRenderer::FillCircle(float x, float y, float rad) {
    // just checks to see if the pixel is within a radius range, uses a bounding box constructed by the diameter

    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;

    View* currentView = &Scene::Views[Scene::ViewCurrent];
    int cx = (int)std::floor(currentView->X);
    int cy = (int)std::floor(currentView->Y);

    Matrix4x4* out = Graphics::ModelViewMatrix.top();
    x += out->Values[12];
    y += out->Values[13];
    x -= cx;
    y -= cy;

    int dst_x1 = x - rad;
    int dst_y1 = y - rad;
    int dst_x2 = x + rad;
    int dst_y2 = y + rad;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_x2 > Graphics::CurrentClip.X + Graphics::CurrentClip.Width)
            dst_x2 = Graphics::CurrentClip.X + Graphics::CurrentClip.Width;
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;

        if (dst_x1 < Graphics::CurrentClip.X)
            dst_x1 = Graphics::CurrentClip.X;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    else {
        if (dst_x2 > (int)Graphics::CurrentRenderTarget->Width)
            dst_x2 = (int)Graphics::CurrentRenderTarget->Width;
        if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
            dst_y2 = (int)Graphics::CurrentRenderTarget->Height;

        if (dst_x1 < 0)
            dst_x1 = 0;
        if (dst_y1 < 0)
            dst_y1 = 0;
    }

    if (dst_x1 >= dst_x2)
        return;
    if (dst_y1 >= dst_y2)
        return;

    int blendFlag = BlendFlag;
    int opacity = Alpha;
    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    int scanLineCount = dst_y2 - dst_y1 + 1;
    Contour* contourPtr = &ContourField[dst_y1];
    while (scanLineCount--) {
        contourPtr->MinX = 0x7FFFFFFF;
        contourPtr->MaxX = -1;
        contourPtr++;
    }

    #define SEEK_MIN(our_x, our_y) if (our_y >= dst_y1 && our_y < dst_y2 && our_x < (cont = &ContourField[our_y])->MinX) \
        cont->MinX = our_x < dst_x1 ? dst_x1 : our_x > (dst_x2 - 1) ? dst_x2 - 1 : our_x;
    #define SEEK_MAX(our_x, our_y) if (our_y >= dst_y1 && our_y < dst_y2 && our_x > (cont = &ContourField[our_y])->MaxX) \
        cont->MaxX = our_x < dst_x1 ? dst_x1 : our_x > (dst_x2 - 1) ? dst_x2 - 1 : our_x;

    Contour* cont;
    int ccx = x, ccy = y;
    int bx = 0, by = rad;
    int bd = 3 - 2 * rad;
    while (bx <= by) {
        if (bd <= 0) {
            bd += 4 * bx + 6;
        }
        else {
            bd += 4 * (bx - by) + 10;
            by--;
        }
        bx++;
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
    }

    Uint32 col = ColRGB;

    #define DRAW_CIRCLE(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        Contour contour = ContourField[dst_y]; \
        if (contour.MaxX < contour.MinX) { \
            dst_strideY += dstStride; \
            continue; \
        } \
        for (int dst_x = contour.MinX >= dst_x1 ? contour.MinX : dst_x1; dst_x < contour.MaxX && dst_x < dst_x2; dst_x++) { \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
        } \
        dst_strideY += dstStride; \
    }

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;
    switch (blendFlag) {
        case 0:
            for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
                Contour contour = ContourField[dst_y];
                if (contour.MaxX < contour.MinX) {
                    dst_strideY += dstStride;
                    continue;
                }
                int dst_min_x = contour.MinX;
                if (dst_min_x < dst_x1)
                    dst_min_x = dst_x1;
                int dst_max_x = contour.MaxX;
                if (dst_max_x > dst_x2 - 1)
                    dst_max_x = dst_x2 - 1;

                Memory::Memset4(&dstPx[dst_min_x + dst_strideY], col, dst_max_x - dst_min_x);
                dst_strideY += dstStride;
            }

            break;
        case 1:
            DRAW_CIRCLE(PixelNoFiltSetTransparent);
            break;
        case 2:
            DRAW_CIRCLE(PixelNoFiltSetAdditive);
            break;
        case 3:
            DRAW_CIRCLE(PixelNoFiltSetSubtract);
            break;
        case 4:
            DRAW_CIRCLE(PixelNoFiltSetMatchEqual);
            break;
        case 5:
            DRAW_CIRCLE(PixelNoFiltSetMatchNotEqual);
            break;
        case 6:
            DRAW_CIRCLE(PixelNoFiltSetFilter);
            break;
    }

    #undef DRAW_CIRCLE
}
PUBLIC STATIC void     SoftwareRenderer::FillEllipse(float x, float y, float w, float h) {

}
PUBLIC STATIC void     SoftwareRenderer::FillRectangle(float x, float y, float w, float h) {
    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;

    View* currentView = &Scene::Views[Scene::ViewCurrent];
    int cx = (int)std::floor(currentView->X);
    int cy = (int)std::floor(currentView->Y);

    Matrix4x4* out = Graphics::ModelViewMatrix.top();
    x += out->Values[12];
    y += out->Values[13];
    x -= cx;
    y -= cy;

    int dst_x1 = x;
    int dst_y1 = y;
    int dst_x2 = x + w;
    int dst_y2 = y + h;

    if (Graphics::CurrentClip.Enabled) {
        if (dst_x2 > Graphics::CurrentClip.X + Graphics::CurrentClip.Width)
            dst_x2 = Graphics::CurrentClip.X + Graphics::CurrentClip.Width;
        if (dst_y2 > Graphics::CurrentClip.Y + Graphics::CurrentClip.Height)
            dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;

        if (dst_x1 < Graphics::CurrentClip.X)
            dst_x1 = Graphics::CurrentClip.X;
        if (dst_y1 < Graphics::CurrentClip.Y)
            dst_y1 = Graphics::CurrentClip.Y;
    }
    else {
        if (dst_x2 > (int)Graphics::CurrentRenderTarget->Width)
            dst_x2 = (int)Graphics::CurrentRenderTarget->Width;
        if (dst_y2 > (int)Graphics::CurrentRenderTarget->Height)
            dst_y2 = (int)Graphics::CurrentRenderTarget->Height;

        if (dst_x1 < 0)
            dst_x1 = 0;
        if (dst_y1 < 0)
            dst_y1 = 0;
    }

    if (dst_x1 >= dst_x2)
        return;
    if (dst_y1 >= dst_y2)
        return;

    int blendFlag = BlendFlag;
    int opacity = Alpha;
    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    Uint32 col = ColRGB;

    #define DRAW_RECT(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        for (int dst_x = dst_x1; dst_x < dst_x2; dst_x++) { \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
        } \
        dst_strideY += dstStride; \
    }

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;
    switch (blendFlag) {
        case 0:
            for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
                Memory::Memset4(&dstPx[dst_x1 + dst_strideY], col, dst_x2 - dst_x1);
                dst_strideY += dstStride;
            }
            break;
        case 1:
            DRAW_RECT(PixelNoFiltSetTransparent);
            break;
        case 2:
            DRAW_RECT(PixelNoFiltSetAdditive);
            break;
        case 3:
            DRAW_RECT(PixelNoFiltSetSubtract);
            break;
        case 4:
            DRAW_RECT(PixelNoFiltSetMatchEqual);
            break;
        case 5:
            DRAW_RECT(PixelNoFiltSetMatchNotEqual);
            break;
        case 6:
            DRAW_RECT(PixelNoFiltSetFilter);
            break;
    }

    #undef DRAW_RECT
}
PUBLIC STATIC void     SoftwareRenderer::FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {
    int x = 0, y = 0;
    View* currentView = &Scene::Views[Scene::ViewCurrent];
    int cx = (int)std::floor(currentView->X);
    int cy = (int)std::floor(currentView->Y);

    Matrix4x4* out = Graphics::ModelViewMatrix.top();
    x += out->Values[12];
    y += out->Values[13];
    x -= cx;
    y -= cy;

    Vector2 vectors[3];
    vectors[0].X = ((int)x1 + x) << 16; vectors[0].Y = ((int)y1 + y) << 16;
    vectors[1].X = ((int)x2 + x) << 16; vectors[1].Y = ((int)y2 + y) << 16;
    vectors[2].X = ((int)x3 + x) << 16; vectors[2].Y = ((int)y3 + y) << 16;
    DrawPolygon(vectors, ColRGB, 3, Alpha, BlendFlag);
}
PUBLIC STATIC void     SoftwareRenderer::FillTriangleBlend(float x1, float y1, float x2, float y2, float x3, float y3, int c1, int c2, int c3) {
    int x = 0, y = 0;
    View* currentView = &Scene::Views[Scene::ViewCurrent];
    int cx = (int)std::floor(currentView->X);
    int cy = (int)std::floor(currentView->Y);

    Matrix4x4* out = Graphics::ModelViewMatrix.top();
    x += out->Values[12];
    y += out->Values[13];
    x -= cx;
    y -= cy;

    int colors[3];
    Vector2 vectors[3];
    vectors[0].X = ((int)x1 + x) << 16; vectors[0].Y = ((int)y1 + y) << 16; colors[0] = ColorMultiply(c1, ColRGB);
    vectors[1].X = ((int)x2 + x) << 16; vectors[1].Y = ((int)y2 + y) << 16; colors[1] = ColorMultiply(c2, ColRGB);
    vectors[2].X = ((int)x3 + x) << 16; vectors[2].Y = ((int)y3 + y) << 16; colors[2] = ColorMultiply(c3, ColRGB);
    DrawPolygonBlend(vectors, colors, 3, Alpha, BlendFlag);
}
PUBLIC STATIC void     SoftwareRenderer::FillQuadBlend(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, int c1, int c2, int c3, int c4) {
    int x = 0, y = 0;
    View* currentView = &Scene::Views[Scene::ViewCurrent];
    int cx = (int)std::floor(currentView->X);
    int cy = (int)std::floor(currentView->Y);

    Matrix4x4* out = Graphics::ModelViewMatrix.top();
    x += out->Values[12];
    y += out->Values[13];
    x -= cx;
    y -= cy;

    int colors[4];
    Vector2 vectors[4];
    vectors[0].X = ((int)x1 + x) << 16; vectors[0].Y = ((int)y1 + y) << 16; colors[0] = ColorMultiply(c1, ColRGB);
    vectors[1].X = ((int)x2 + x) << 16; vectors[1].Y = ((int)y2 + y) << 16; colors[1] = ColorMultiply(c2, ColRGB);
    vectors[2].X = ((int)x3 + x) << 16; vectors[2].Y = ((int)y3 + y) << 16; colors[2] = ColorMultiply(c3, ColRGB);
    vectors[3].X = ((int)x4 + x) << 16; vectors[3].Y = ((int)y4 + y) << 16; colors[3] = ColorMultiply(c4, ColRGB);
    DrawPolygonBlend(vectors, colors, 4, Alpha, BlendFlag);
}

void DrawSpriteImage(Texture* texture, int x, int y, int w, int h, int sx, int sy, int flipFlag, int blendFlag, int opacity) {
    Uint32* srcPx = (Uint32*)texture->Pixels;
    Uint32  srcStride = texture->Width;
    Uint32* srcPxLine;

    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;
    Uint32* dstPxLine;

    int src_x1 = sx;
    int src_y1 = sy;
    int src_x2 = sx + w - 1;
    int src_y2 = sy + h - 1;

    int dst_x1 = x;
    int dst_y1 = y;
    int dst_x2 = x + w;
    int dst_y2 = y + h;

    if (Alpha != 0 && blendFlag == BlendFlag_OPAQUE)
        blendFlag = BlendFlag_TRANSPARENT;
    if (!Graphics::TextureBlend)
        blendFlag = BlendFlag_OPAQUE;

    if (Graphics::CurrentClip.Enabled) {
        int
            clip_x1 = Graphics::CurrentClip.X,
            clip_y1 = Graphics::CurrentClip.Y,
            clip_x2 = Graphics::CurrentClip.X + Graphics::CurrentClip.Width,
            clip_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;

        if (dst_x2 > clip_x2)
            dst_x2 = clip_x2;
        if (dst_y2 > clip_y2)
            dst_y2 = clip_y2;

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
    }
    else {
        int
            clip_x2 = (int)Graphics::CurrentRenderTarget->Width,
            clip_y2 = (int)Graphics::CurrentRenderTarget->Height;
        if (dst_x2 > clip_x2)
            dst_x2 = clip_x2;
        if (dst_y2 > clip_y2)
            dst_y2 = clip_y2;

        if (dst_x1 < 0) {
            src_x1 += -dst_x1;
            src_x2 -= -dst_x1;
            dst_x1 = 0;
        }
        if (dst_y1 < 0) {
            src_y1 += -dst_y1;
            src_y2 -= -dst_y1;
            dst_y1 = 0;
        }
    }

    if (dst_x1 >= dst_x2)
        return;
    if (dst_y1 >= dst_y2)
        return;

    #define DRAW_PLACEPIXEL(pixelFunction) \
        if ((color = srcPxLine[src_x]) & 0xFF000000U) \
            pixelFunction(&color, &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);
    #define DRAW_PLACEPIXEL_PAL(pixelFunction) \
        if ((color = srcPxLine[src_x])) \
            pixelFunction(&index[color], &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);

    #define DRAW_NOFLIP(pixelFunction, placePixelMacro) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        srcPxLine = srcPx + src_strideY; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; dst_x++, src_x++) { \
            placePixelMacro(pixelFunction) \
        } \
        dst_strideY += dstStride; src_strideY += srcStride; \
    }
    #define DRAW_FLIPX(pixelFunction, placePixelMacro) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        srcPxLine = srcPx + src_strideY; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; dst_x++, src_x--) { \
            placePixelMacro(pixelFunction) \
        } \
        dst_strideY += dstStride; src_strideY += srcStride; \
    }
    #define DRAW_FLIPY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        srcPxLine = srcPx + src_strideY; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; dst_x++, src_x++) { \
            placePixelMacro(pixelFunction) \
        } \
        dst_strideY += dstStride; src_strideY -= srcStride; \
    }
    #define DRAW_FLIPXY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        srcPxLine = srcPx + src_strideY; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; dst_x++, src_x--) { \
            placePixelMacro(pixelFunction) \
        } \
        dst_strideY += dstStride; src_strideY -= srcStride; \
    }

    #define BLENDFLAGS(flipMacro, placePixelMacro) \
        switch (blendFlag) { \
            case 0: \
                flipMacro(PixelNoFiltSetOpaque, placePixelMacro); \
                break; \
            case 1: \
                flipMacro(PixelNoFiltSetTransparent, placePixelMacro); \
                break; \
            case 2: \
                flipMacro(PixelNoFiltSetAdditive, placePixelMacro); \
                break; \
            case 3: \
                flipMacro(PixelNoFiltSetSubtract, placePixelMacro); \
                break; \
            case 4: \
                flipMacro(PixelNoFiltSetMatchEqual, placePixelMacro); \
                break; \
            case 5: \
                flipMacro(PixelNoFiltSetMatchNotEqual, placePixelMacro); \
                break; \
            case 6: \
                flipMacro(PixelNoFiltSetFilter, placePixelMacro); \
                break; \
        }

    Uint32 color;
    Uint32* index;
    int dst_strideY, src_strideY;
    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    if (Graphics::UsePalettes && texture->Paletted) {
        switch (flipFlag) {
            case 0:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y1 * srcStride;
                BLENDFLAGS(DRAW_NOFLIP, DRAW_PLACEPIXEL_PAL);
                break;
            case 1:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y1 * srcStride;
                BLENDFLAGS(DRAW_FLIPX, DRAW_PLACEPIXEL_PAL);
                break;
            case 2:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y2 * srcStride;
                BLENDFLAGS(DRAW_FLIPY, DRAW_PLACEPIXEL_PAL);
                break;
            case 3:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y2 * srcStride;
                BLENDFLAGS(DRAW_FLIPXY, DRAW_PLACEPIXEL_PAL);
                break;
        }
    }
    else {
        switch (flipFlag) {
            case 0:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y1 * srcStride;
                BLENDFLAGS(DRAW_NOFLIP, DRAW_PLACEPIXEL);
                break;
            case 1:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y1 * srcStride;
                BLENDFLAGS(DRAW_FLIPX, DRAW_PLACEPIXEL);
                break;
            case 2:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y2 * srcStride;
                BLENDFLAGS(DRAW_FLIPY, DRAW_PLACEPIXEL);
                break;
            case 3:
                dst_strideY = dst_y1 * dstStride;
                src_strideY = src_y2 * srcStride;
                BLENDFLAGS(DRAW_FLIPXY, DRAW_PLACEPIXEL);
                break;
        }
    }

    #undef DRAW_PLACEPIXEL
    #undef DRAW_PLACEPIXEL_PAL
    #undef DRAW_NOFLIP
    #undef DRAW_FLIPX
    #undef DRAW_FLIPY
    #undef DRAW_FLIPXY
    #undef BLENDFLAGS
}
void DrawSpriteImageTransformed(Texture* texture, int x, int y, int offx, int offy, int w, int h, int sx, int sy, int sw, int sh, int flipFlag, int rotation, int blendFlag, int opacity) {
    Uint32* srcPx = (Uint32*)texture->Pixels;
    Uint32  srcStride = texture->Width;
    // Uint32* srcPxLine;

    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;
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
		case 1: _x1 = -offx - w; _x2 = -offx; break;
		case 2: _y1 = -offy - h; _y2 = -offy; break;
        case 3:
			_x1 = -offx - w; _x2 = -offx;
			_y1 = -offy - h; _y2 = -offy;
    }

    int dst_x1 = _x1;
    int dst_y1 = _y1;
    int dst_x2 = _x2;
    int dst_y2 = _y2;

    if (Alpha != 0 && blendFlag == BlendFlag_OPAQUE)
        blendFlag = BlendFlag_TRANSPARENT;
    if (!Graphics::TextureBlend)
        blendFlag = BlendFlag_OPAQUE;

    #define SET_MIN(a, b) if (a > b) a = b;
    #define SET_MAX(a, b) if (a < b) a = b;

    int px, py, cx, cy;

    py = _y1;
    {
        px = _x1;
        cx = (px * cos - py * sin); SET_MIN(dst_x1, cx); SET_MAX(dst_x2, cx);
        cy = (px * sin + py * cos); SET_MIN(dst_y1, cy); SET_MAX(dst_y2, cy);

        px = _x2;
        cx = (px * cos - py * sin); SET_MIN(dst_x1, cx); SET_MAX(dst_x2, cx);
        cy = (px * sin + py * cos); SET_MIN(dst_y1, cy); SET_MAX(dst_y2, cy);
    }

    py = _y2;
    {
        px = _x1;
        cx = (px * cos - py * sin); SET_MIN(dst_x1, cx); SET_MAX(dst_x2, cx);
        cy = (px * sin + py * cos); SET_MIN(dst_y1, cy); SET_MAX(dst_y2, cy);

        px = _x2;
        cx = (px * cos - py * sin); SET_MIN(dst_x1, cx); SET_MAX(dst_x2, cx);
        cy = (px * sin + py * cos); SET_MIN(dst_y1, cy); SET_MAX(dst_y2, cy);
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

    if (Graphics::CurrentClip.Enabled) {
        int
            clip_x1 = Graphics::CurrentClip.X,
            clip_y1 = Graphics::CurrentClip.Y,
            clip_x2 = Graphics::CurrentClip.X + Graphics::CurrentClip.Width,
            clip_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;

        if (dst_x1 < clip_x1)
            dst_x1 = clip_x1;
        if (dst_y1 < clip_y1)
            dst_y1 = clip_y1;
        if (dst_x2 > clip_x2)
            dst_x2 = clip_x2;
        if (dst_y2 > clip_y2)
            dst_y2 = clip_y2;
    }
    else {
        int
            clip_x2 = (int)Graphics::CurrentRenderTarget->Width,
            clip_y2 = (int)Graphics::CurrentRenderTarget->Height;
        if (dst_x1 < 0)
            dst_x1 = 0;
        if (dst_y1 < 0)
            dst_y1 = 0;
        if (dst_x2 > clip_x2)
            dst_x2 = clip_x2;
        if (dst_y2 > clip_y2)
            dst_y2 = clip_y2;
    }

    if (dst_x1 >= dst_x2)
        return;
    if (dst_y1 >= dst_y2)
        return;

    #define DRAW_PLACEPIXEL(pixelFunction) \
        if ((color = srcPx[src_x + src_strideY]) & 0xFF000000U) \
            pixelFunction(&color, &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);
    #define DRAW_PLACEPIXEL_PAL(pixelFunction) \
        if ((color = srcPx[src_x + src_strideY])) \
            pixelFunction(&index[color], &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);

    #define DRAW_NOFLIP(pixelFunction, placePixelMacro) for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
        i_y_rsin = -i_y * rsin; \
        i_y_rcos =  i_y * rcos; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
            src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
            src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
            if (src_x >= _x1 && src_y >= _y1 && \
                src_x <  _x2 && src_y <  _y2) { \
                src_x       = (src_x1 + (src_x - _x1) * sw / w); \
                src_strideY = (src_y1 + (src_y - _y1) * sh / h) * srcStride; \
                placePixelMacro(pixelFunction); \
            } \
        } \
        dst_strideY += dstStride; \
    }
    #define DRAW_FLIPX(pixelFunction, placePixelMacro) for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
        i_y_rsin = -i_y * rsin; \
        i_y_rcos =  i_y * rcos; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
            src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
            src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
            if (src_x >= _x1 && src_y >= _y1 && \
                src_x <  _x2 && src_y <  _y2) { \
                src_x       = (src_x2 - (src_x - _x1) * sw / w); \
                src_strideY = (src_y1 + (src_y - _y1) * sh / h) * srcStride; \
                placePixelMacro(pixelFunction); \
            } \
        } \
        dst_strideY += dstStride; \
    }
    #define DRAW_FLIPY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
        i_y_rsin = -i_y * rsin; \
        i_y_rcos =  i_y * rcos; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
            src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
            src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
            if (src_x >= _x1 && src_y >= _y1 && \
                src_x <  _x2 && src_y <  _y2) { \
                src_x       = (src_x1 + (src_x - _x1) * sw / w); \
                src_strideY = (src_y2 - (src_y - _y1) * sh / h) * srcStride; \
                placePixelMacro(pixelFunction); \
            } \
        } \
        dst_strideY += dstStride; \
    }
    #define DRAW_FLIPXY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
        i_y_rsin = -i_y * rsin; \
        i_y_rcos =  i_y * rcos; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
            src_x = (i_x * rcos + i_y_rsin) >> TRIG_TABLE_BITS; \
            src_y = (i_x * rsin + i_y_rcos) >> TRIG_TABLE_BITS; \
            if (src_x >= _x1 && src_y >= _y1 && \
                src_x <  _x2 && src_y <  _y2) { \
                src_x       = (src_x2 - (src_x - _x1) * sw / w); \
                src_strideY = (src_y2 - (src_y - _y1) * sh / h) * srcStride; \
                placePixelMacro(pixelFunction); \
            } \
        } \
        dst_strideY += dstStride; \
    }

    #define BLENDFLAGS(flipMacro, placePixelMacro) \
        switch (blendFlag) { \
            case 0: \
                flipMacro(PixelNoFiltSetOpaque, placePixelMacro); \
                break; \
            case 1: \
                flipMacro(PixelNoFiltSetTransparent, placePixelMacro); \
                break; \
            case 2: \
                flipMacro(PixelNoFiltSetAdditive, placePixelMacro); \
                break; \
            case 3: \
                flipMacro(PixelNoFiltSetSubtract, placePixelMacro); \
                break; \
            case 4: \
                flipMacro(PixelNoFiltSetMatchEqual, placePixelMacro); \
                break; \
            case 5: \
                flipMacro(PixelNoFiltSetMatchNotEqual, placePixelMacro); \
                break; \
            case 6: \
                flipMacro(PixelNoFiltSetFilter, placePixelMacro); \
                break; \
        }

    Uint32 color;
    Uint32* index;
    int i_y_rsin, i_y_rcos;
    int dst_strideY, src_strideY;
    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    if (Graphics::UsePalettes && texture->Paletted) {
        switch (flipFlag) {
            case 0:
                dst_strideY = dst_y1 * dstStride;
                BLENDFLAGS(DRAW_NOFLIP, DRAW_PLACEPIXEL_PAL);
                break;
            case 1:
                dst_strideY = dst_y1 * dstStride;
                BLENDFLAGS(DRAW_FLIPX, DRAW_PLACEPIXEL_PAL);
                break;
            case 2:
                dst_strideY = dst_y1 * dstStride;
                BLENDFLAGS(DRAW_FLIPY, DRAW_PLACEPIXEL_PAL);
                break;
            case 3:
                dst_strideY = dst_y1 * dstStride;
                BLENDFLAGS(DRAW_FLIPXY, DRAW_PLACEPIXEL_PAL);
                break;
        }
    }
    else {
        switch (flipFlag) {
            case 0:
                dst_strideY = dst_y1 * dstStride;
                BLENDFLAGS(DRAW_NOFLIP, DRAW_PLACEPIXEL);
                break;
            case 1:
                dst_strideY = dst_y1 * dstStride;
                BLENDFLAGS(DRAW_FLIPX, DRAW_PLACEPIXEL);
                break;
            case 2:
                dst_strideY = dst_y1 * dstStride;
                BLENDFLAGS(DRAW_FLIPY, DRAW_PLACEPIXEL);
                break;
            case 3:
                dst_strideY = dst_y1 * dstStride;
                BLENDFLAGS(DRAW_FLIPXY, DRAW_PLACEPIXEL);
                break;
        }
    }

    #undef DRAW_PLACEPIXEL
    #undef DRAW_PLACEPIXEL_PAL
    #undef DRAW_NOFLIP
    #undef DRAW_FLIPX
    #undef DRAW_FLIPY
    #undef DRAW_FLIPXY
    #undef BLENDFLAGS
}

PUBLIC STATIC void     SoftwareRenderer::DrawTexture(Texture* texture, float sx, float sy, float sw, float sh, float x, float y, float w, float h) {
    View* currentView = &Scene::Views[Scene::ViewCurrent];
    int cx = (int)std::floor(currentView->X);
    int cy = (int)std::floor(currentView->Y);

    Matrix4x4* out = Graphics::ModelViewMatrix.top();
    x += out->Values[12];
    y += out->Values[13];
    x -= cx;
    y -= cy;

    DrawSpriteImage(texture,
        x, y, (int)sw, (int)sh, (int)sx, (int)sy, 0, BlendFlag, Alpha);
}
PUBLIC STATIC void     SoftwareRenderer::DrawSprite(ISprite* sprite, int animation, int frame, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation) {
    if (Graphics::SpriteRangeCheck(sprite, animation, frame)) return;

    AnimFrame frameStr = sprite->Animations[animation].Frames[frame];
    Texture* texture = sprite->Spritesheets[frameStr.SheetNumber];

    View* currentView = &Scene::Views[Scene::ViewCurrent];
    int cx = (int)std::floor(currentView->X);
    int cy = (int)std::floor(currentView->Y);

    Matrix4x4* out = Graphics::ModelViewMatrix.top();
    x += out->Values[12];
    y += out->Values[13];
    x -= cx;
    y -= cy;

    int flipFlag = (int)flipX | ((int)flipY << 1);
    if (rotation != 0.0f || scaleW != 1.0f || scaleH != 1.0f) {
        int rot = (int)(rotation * TRIG_TABLE_HALF / M_PI) & TRIG_TABLE_MASK;
        DrawSpriteImageTransformed(texture,
            x, y,
            frameStr.OffsetX * scaleW, frameStr.OffsetY * scaleH,
            frameStr.Width * scaleW, frameStr.Height * scaleH,

            frameStr.X, frameStr.Y,
            frameStr.Width, frameStr.Height,
            flipFlag, rot,
            BlendFlag, Alpha);
        return;
    }
    switch (flipFlag) {
        case 0:
            DrawSpriteImage(texture,
                x + frameStr.OffsetX,
                y + frameStr.OffsetY,
                frameStr.Width, frameStr.Height, frameStr.X, frameStr.Y, flipFlag, BlendFlag, Alpha);
            break;
        case 1:
            DrawSpriteImage(texture,
                x - frameStr.OffsetX - frameStr.Width,
                y + frameStr.OffsetY,
                frameStr.Width, frameStr.Height, frameStr.X, frameStr.Y, flipFlag, BlendFlag, Alpha);
            break;
        case 2:
            DrawSpriteImage(texture,
                x + frameStr.OffsetX,
                y - frameStr.OffsetY - frameStr.Height,
                frameStr.Width, frameStr.Height, frameStr.X, frameStr.Y, flipFlag, BlendFlag, Alpha);
            break;
        case 3:
            DrawSpriteImage(texture,
                x - frameStr.OffsetX - frameStr.Width,
                y - frameStr.OffsetY - frameStr.Height,
                frameStr.Width, frameStr.Height, frameStr.X, frameStr.Y, flipFlag, BlendFlag, Alpha);
            break;
    }
}
PUBLIC STATIC void     SoftwareRenderer::DrawSpritePart(ISprite* sprite, int animation, int frame, int sx, int sy, int sw, int sh, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation) {
	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) return;

    AnimFrame frameStr = sprite->Animations[animation].Frames[frame];
    Texture* texture = sprite->Spritesheets[frameStr.SheetNumber];

    View* currentView = &Scene::Views[Scene::ViewCurrent];
    int cx = (int)std::floor(currentView->X);
    int cy = (int)std::floor(currentView->Y);

    Matrix4x4* out = Graphics::ModelViewMatrix.top();
    x += out->Values[12];
    y += out->Values[13];
    x -= cx;
    y -= cy;

    if (sw >= frameStr.Width - sx)
        sw  = frameStr.Width - sx;
    if (sh >= frameStr.Height - sy)
        sh  = frameStr.Height - sy;

    int flipFlag = (int)flipX | ((int)flipY << 1);
    if (rotation != 0.0f || scaleW != 1.0f || scaleH != 1.0f) {
        int rot = (int)(rotation * TRIG_TABLE_HALF / M_PI) & TRIG_TABLE_MASK;
        DrawSpriteImageTransformed(texture,
            x, y,
            (frameStr.OffsetX + sx) * scaleW, (frameStr.OffsetY + sy) * scaleH,
            sw * scaleW, sh * scaleH,

            frameStr.X + sx, frameStr.Y + sy,
            sw, sh,
            flipFlag, rot,
            BlendFlag, Alpha);
        return;
    }
    switch (flipFlag) {
        case 0:
            DrawSpriteImage(texture,
                x + frameStr.OffsetX + sx,
                y + frameStr.OffsetY + sy,
                sw, sh, frameStr.X + sx, frameStr.Y + sy, flipFlag, BlendFlag, Alpha);
            break;
        case 1:
            DrawSpriteImage(texture,
                x - frameStr.OffsetX - sw - sx,
                y + frameStr.OffsetY + sy,
                sw, sh, frameStr.X + sx, frameStr.Y + sy, flipFlag, BlendFlag, Alpha);
            break;
        case 2:
            DrawSpriteImage(texture,
                x + frameStr.OffsetX + sx,
                y - frameStr.OffsetY - sh - sy,
                sw, sh, frameStr.X + sx, frameStr.Y + sy, flipFlag, BlendFlag, Alpha);
            break;
        case 3:
            DrawSpriteImage(texture,
                x - frameStr.OffsetX - sw - sx,
                y - frameStr.OffsetY - sh - sy,
                sw, sh, frameStr.X + sx, frameStr.Y + sy, flipFlag, BlendFlag, Alpha);
            break;
    }
}

// Default Tile Display Line setup
PUBLIC STATIC void     SoftwareRenderer::DrawTile(int tile, int x, int y, bool flipX, bool flipY) {

}
PUBLIC STATIC void     SoftwareRenderer::DrawSceneLayer_InitTileScanLines(SceneLayer* layer, View* currentView) {
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
                info->Position = (info->Offset + ((viewX + layerOffsetX) * info->RelativeParallax)) >> 8;
                info->Position %= layerWidth;
                if (info->Position < 0)
                    info->Position += layerWidth;
                info++;
            }

            // Create scan lines
            Sint64 scrollOffset = Scene::Frame * layer->ConstantY;
            Sint64 scrollLine = (scrollOffset + ((viewY + layerOffsetY) * layer->RelativeY)) >> 8;
                   scrollLine %= layerHeight;
            if (scrollLine < 0)
                scrollLine += layerHeight;

            int* deformValues;
            Uint8* parallaxIndex;
            TileScanLine* scanLine;
            const int maxDeformLineMask = (MAX_DEFORM_LINES >> 1) - 1;

            scanLine = &TileScanLineBuffer[0];
            parallaxIndex = &layer->ScrollIndexes[scrollLine];
            deformValues = &layer->DeformSetA[(scrollLine + layer->DeformOffsetA) & maxDeformLineMask];
            for (int i = 0; i < layer->DeformSplitLine; i++) {
                // Set scan line start positions
                info = &layer->ScrollInfos[*parallaxIndex];
                scanLine->SrcX = info->Position;
                if (info->CanDeform)
                    scanLine->SrcX += *deformValues;
                scanLine->SrcX <<= 16;
                scanLine->SrcY = scrollLine << 16;

                scanLine->DeltaX = 0x10000;
                scanLine->DeltaY = 0x0000;

                // Iterate lines
                // NOTE: There is no protection from over-reading deform indexes past 512 here.
                scanLine++;
                scrollLine++;
                deformValues++;

                // If we've reach the last line of the layer, return to the first.
                if (scrollLine == layerHeight) {
                    scrollLine = 0;
                    parallaxIndex = &layer->ScrollIndexes[scrollLine];
                }
                else {
                    parallaxIndex++;
                }
            }

            deformValues = &layer->DeformSetB[(scrollLine + layer->DeformOffsetB) & maxDeformLineMask];
            for (int i = layer->DeformSplitLine; i < viewHeight; i++) {
                // Set scan line start positions
                info = &layer->ScrollInfos[*parallaxIndex];
                scanLine->SrcX = info->Position;
                if (info->CanDeform)
                    scanLine->SrcX += *deformValues;
                scanLine->SrcX <<= 16;
                scanLine->SrcY = scrollLine << 16;

                scanLine->DeltaX = 0x10000;
                scanLine->DeltaY = 0x0000;

                // Iterate lines
                // NOTE: There is no protection from over-reading deform indexes past 512 here.
                scanLine++;
                scrollLine++;
                deformValues++;

                // If we've reach the last line of the layer, return to the first.
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
            Sint64 scrollPositionX = ((scrollOffset + (((int)currentView->X + layer->OffsetX) * layer->RelativeY)) >> 8) & 0xFFFF;
                   scrollPositionX %= layer->Width;
                   scrollPositionX <<= 16;
            Sint64 scrollPositionY = ((scrollOffset + (((int)currentView->Y + layer->OffsetY) * layer->RelativeY)) >> 8) & 0xFFFF;
                   scrollPositionY %= layer->Height;
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
PUBLIC STATIC void     SoftwareRenderer::DrawSceneLayer_HorizontalParallax(SceneLayer* layer, View* currentView) {
    int dst_x1 = 0;
    int dst_y1 = 0;
    int dst_x2 = (int)Graphics::CurrentRenderTarget->Width;
    int dst_y2 = (int)Graphics::CurrentRenderTarget->Height;

    Uint32  srcStride = 0;

    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;
    Uint32* dstPxLine;

    if (Graphics::CurrentClip.Enabled) {
        dst_x1 = Graphics::CurrentClip.X;
        dst_y1 = Graphics::CurrentClip.Y;
        dst_x2 = Graphics::CurrentClip.X + Graphics::CurrentClip.Width;
        dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
    }

    if (dst_x1 >= dst_x2 || dst_y1 >= dst_y2)
        return;

    bool canCollide = (layer->Flags & SceneLayer::FLAGS_COLLIDEABLE);

    int layerWidthInBits = layer->WidthInBits;
    // int layerWidthTileMask = layer->WidthMask;
    // int layerHeightTileMask = layer->HeightMask;
    int layerWidthInPixels = layer->Width * 16;
    // int layerHeightInPixels = layer->Height * 16;
    int layerWidth = layer->Width;
    // int layerHeight = layer->Height;
    int sourceTileCellX, sourceTileCellY;
    TileSpriteInfo info;
    AnimFrame frameStr;
    Texture* texture;

    int blendFlag = BlendFlag;
    int opacity = Alpha;
    if (!Graphics::TextureBlend) {
        blendFlag = BlendFlag_OPAQUE;
        opacity = 0;
    }

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];

    Uint32* tile;
    Uint32* color;
    Uint32* index;
    int dst_strideY = dst_y1 * dstStride;

    int maxTileDraw = ((int)currentView->Stride >> 4) - 1;

    vector<Uint32> srcStrides;
    vector<Uint32*> tileSources;
    vector<Uint8> isPalettedSources;
    srcStrides.resize(Scene::TileSpriteInfos.size());
    tileSources.resize(Scene::TileSpriteInfos.size());
    isPalettedSources.resize(Scene::TileSpriteInfos.size());
    for (size_t i = 0; i < Scene::TileSpriteInfos.size(); i++) {
        info = Scene::TileSpriteInfos[i];
        frameStr = info.Sprite->Animations[info.AnimationIndex].Frames[info.FrameIndex];
        texture = info.Sprite->Spritesheets[frameStr.SheetNumber];
        srcStrides[i] = srcStride = texture->Width;
        tileSources[i] = (&((Uint32*)texture->Pixels)[frameStr.X + frameStr.Y * srcStride]);
        isPalettedSources[i] = Graphics::UsePalettes && texture->Paletted;
    }

    Uint32 DRAW_COLLISION = 0;
    int c_pixelsOfTileRemaining, tileFlipOffset;
	TileConfig* baseTileCfg = Scene::ShowTileCollisionFlag == 2 ? Scene::TileCfgB : Scene::TileCfgA;

    void (*pixelFunction)(Uint32*, Uint32*, int, int*, int*) = NULL;
    switch (blendFlag) {
        case BlendFlag_OPAQUE:
            pixelFunction = PixelNoFiltSetOpaque;
            break;
        case BlendFlag_TRANSPARENT:
            pixelFunction = PixelNoFiltSetTransparent;
            break;
        case BlendFlag_ADDITIVE:
            pixelFunction = PixelNoFiltSetAdditive;
            break;
        case BlendFlag_SUBTRACT:
            pixelFunction = PixelNoFiltSetSubtract;
            break;
        case BlendFlag_MATCH_EQUAL:
            pixelFunction = PixelNoFiltSetMatchEqual;
            break;
        case BlendFlag_MATCH_NOT_EQUAL:
            pixelFunction = PixelNoFiltSetMatchNotEqual;
            break;
        case BlendFlag_FILTER:
            pixelFunction = PixelNoFiltSetFilter;
            break;
    }

    int j;
    TileScanLine* tScanLine = &TileScanLineBuffer[dst_y1];
    for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
        tScanLine->SrcX >>= 16;
        tScanLine->SrcY >>= 16;
        dstPxLine = dstPx + dst_strideY;

        if (tScanLine->SrcX < 0)
            tScanLine->SrcX += layerWidthInPixels;
        else if (tScanLine->SrcX >= layerWidthInPixels)
            tScanLine->SrcX -= layerWidthInPixels;

        int dst_x = dst_x1, c_dst_x = dst_x1;
        int pixelsOfTileRemaining;
        Sint64 srcX = tScanLine->SrcX, srcY = tScanLine->SrcY;
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0];

        // Draw leftmost tile in scanline
        int srcTX = srcX & 15;
        int srcTY = srcY & 15;
        sourceTileCellX = (srcX >> 4);
        sourceTileCellY = (srcY >> 4);
        c_pixelsOfTileRemaining = srcTX;
        pixelsOfTileRemaining = 16 - srcTX;
        tile = &layer->Tiles[sourceTileCellX + (sourceTileCellY << layerWidthInBits)];

        if ((*tile & TILE_IDENT_MASK) != Scene::EmptyTile) {
            int tileID = *tile & TILE_IDENT_MASK;

            if (Scene::ShowTileCollisionFlag && Scene::TileCfgA) {
                c_dst_x = dst_x;
                if (Scene::ShowTileCollisionFlag == 1)
                    DRAW_COLLISION = (*tile & TILE_COLLA_MASK) >> 28;
                else if (Scene::ShowTileCollisionFlag == 2)
                    DRAW_COLLISION = (*tile & TILE_COLLB_MASK) >> 26;

                switch (DRAW_COLLISION) {
                    case 1: DRAW_COLLISION = 0xFFFFFF00U; break;
                    case 2: DRAW_COLLISION = 0xFFFF0000U; break;
                    case 3: DRAW_COLLISION = 0xFFFFFFFFU; break;
                }
            }

            // If y-flipped
            if ((*tile & TILE_FLIPY_MASK))
                srcTY ^= 15;
            // If x-flipped
            if ((*tile & TILE_FLIPX_MASK)) {
                srcTX ^= 15;
                color = &tileSources[tileID][srcTX + srcTY * srcStrides[tileID]];
                if (isPalettedSources[tileID]) {
                    while (pixelsOfTileRemaining) {
                        if (*color)
                            pixelFunction(&index[*color], &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);
                        pixelsOfTileRemaining--;
                        dst_x++;
                        color--;
                    }
                }
                else {
                    while (pixelsOfTileRemaining) {
                        if (*color & 0xFF000000U)
                            pixelFunction(color, &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);
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
                        if (*color)
                            pixelFunction(&index[*color], &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);
                        pixelsOfTileRemaining--;
                        dst_x++;
                        color++;
                    }
                }
                else {
                    while (pixelsOfTileRemaining) {
                        if (*color & 0xFF000000U)
                            pixelFunction(color, &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);
                        pixelsOfTileRemaining--;
                        dst_x++;
                        color++;
                    }
                }
            }

            if (canCollide && DRAW_COLLISION) {
                tileFlipOffset = (
                    ( (!!(*tile & TILE_FLIPY_MASK)) << 1 ) | (!!(*tile & TILE_FLIPX_MASK))
                ) * Scene::TileCount;

                bool flipY = !!(*tile & TILE_FLIPY_MASK);
                bool isCeiling = !!baseTileCfg[tileID].IsCeiling;
                TileConfig* tile = (&baseTileCfg[tileID] + tileFlipOffset);
                for (int gg = c_pixelsOfTileRemaining; gg < 16; gg++) {
                    if ((flipY == isCeiling && (srcY & 15) >= tile->CollisionTop[gg] && tile->CollisionTop[gg] < 0xF0) ||
                        (flipY != isCeiling && (srcY & 15) <= tile->CollisionBottom[gg] && tile->CollisionBottom[gg] < 0xF0)) {
                        PixelNoFiltSetOpaque(&DRAW_COLLISION, &dstPxLine[c_dst_x], 0, NULL, NULL);
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
            if (sourceTileCellX == layerWidth) {
                sourceTileCellX = 0;
                tile -= layerWidth;
            }

            if (Scene::ShowTileCollisionFlag && Scene::TileCfgA) {
                c_dst_x = dst_x;
                if (Scene::ShowTileCollisionFlag == 1)
                    DRAW_COLLISION = (*tile & TILE_COLLA_MASK) >> 28;
                else if (Scene::ShowTileCollisionFlag == 2)
                    DRAW_COLLISION = (*tile & TILE_COLLB_MASK) >> 26;

                switch (DRAW_COLLISION) {
                    case 1: DRAW_COLLISION = 0xFFFFFF00U; break;
                    case 2: DRAW_COLLISION = 0xFFFF0000U; break;
                    case 3: DRAW_COLLISION = 0xFFFFFFFFU; break;
                }
            }

            int srcTYb = srcTY;
            if ((*tile & TILE_IDENT_MASK) != Scene::EmptyTile) {
                int tileID = *tile & TILE_IDENT_MASK;
                // If y-flipped
                if ((*tile & TILE_FLIPY_MASK))
                    srcTYb ^= 15;
                // If x-flipped
                if ((*tile & TILE_FLIPX_MASK)) {
                    color = &tileSources[tileID][srcTYb * srcStrides[tileID]];
                    if (isPalettedSources[tileID]) {
                        #define UNLOOPED(n, k) if (color[n]) { pixelFunction(&index[color[n]], &dstPxLine[dst_x + k], opacity, multTableAt, multSubTableAt); }
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
                        #define UNLOOPED(n, k) if (color[n] & 0xFF000000U) { pixelFunction(&color[n], &dstPxLine[dst_x + k], opacity, multTableAt, multSubTableAt); }
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
                        #define UNLOOPED(n, k) if (color[n]) { pixelFunction(&index[color[n]], &dstPxLine[dst_x + k], opacity, multTableAt, multSubTableAt); }
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
                        #define UNLOOPED(n, k) if (color[n] & 0xFF000000U) { pixelFunction(&color[n], &dstPxLine[dst_x + k], opacity, multTableAt, multSubTableAt); }
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
                    tileFlipOffset = (
                        ( (!!(*tile & TILE_FLIPY_MASK)) << 1 ) | (!!(*tile & TILE_FLIPX_MASK))
                    ) * Scene::TileCount;

                    bool flipY = !!(*tile & TILE_FLIPY_MASK);
                    bool isCeiling = !!baseTileCfg[tileID].IsCeiling;
                    TileConfig* tile = (&baseTileCfg[tileID] + tileFlipOffset);
                    for (int gg = 0; gg < 16; gg++) {
                        if ((flipY == isCeiling && (srcY & 15) >= tile->CollisionTop[gg] && tile->CollisionTop[gg] < 0xF0) ||
                            (flipY != isCeiling && (srcY & 15) <= tile->CollisionBottom[gg] && tile->CollisionBottom[gg] < 0xF0)) {
                            PixelNoFiltSetOpaque(&DRAW_COLLISION, &dstPxLine[c_dst_x], 0, NULL, NULL);
                        }
                        c_dst_x++;
                    }
                }
            }
        }
        srcX += maxTileDraw * 16;

        tScanLine++;
        dst_strideY += dstStride;
    }
}
PUBLIC STATIC void     SoftwareRenderer::DrawSceneLayer_VerticalParallax(SceneLayer* layer, View* currentView) {

}
PUBLIC STATIC void     SoftwareRenderer::DrawSceneLayer_CustomTileScanLines(SceneLayer* layer, View* currentView) {
    int dst_x1 = 0;
    int dst_y1 = 0;
    int dst_x2 = (int)Graphics::CurrentRenderTarget->Width;
    int dst_y2 = (int)Graphics::CurrentRenderTarget->Height;

    // Uint32* srcPx = NULL;
    Uint32  srcStride = 0;
    // Uint32* srcPxLine;

    Uint32* dstPx = (Uint32*)Graphics::CurrentRenderTarget->Pixels;
    Uint32  dstStride = Graphics::CurrentRenderTarget->Width;
    Uint32* dstPxLine;

    if (Graphics::CurrentClip.Enabled) {
        dst_x1 = Graphics::CurrentClip.X;
        dst_y1 = Graphics::CurrentClip.Y;
        dst_x2 = Graphics::CurrentClip.X + Graphics::CurrentClip.Width;
        dst_y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height;
    }

    if (dst_x1 >= dst_x2 || dst_y1 >= dst_y2)
        return;

    int layerWidthInBits = layer->WidthInBits;
    int layerWidthTileMask = layer->WidthMask;
    int layerHeightTileMask = layer->HeightMask;
    int tile, sourceTileCellX, sourceTileCellY;
    TileSpriteInfo info;
    AnimFrame frameStr;
    Texture* texture;

    int blendFlag = BlendFlag;
    if (!Graphics::TextureBlend)
        blendFlag = BlendFlag_OPAQUE;
    if (FilterTable != &FilterColor[0])
        blendFlag = BlendFlag_FILTER;

    Uint32 color;
    Uint32* index;
    int dst_strideY = dst_y1 * dstStride;

    vector<Uint32> srcStrides;
    vector<Uint32*> tileSources;
    vector<Uint8> isPalettedSources;
    srcStrides.resize(Scene::TileSpriteInfos.size());
    tileSources.resize(Scene::TileSpriteInfos.size());
    isPalettedSources.resize(Scene::TileSpriteInfos.size());
    for (size_t i = 0; i < Scene::TileSpriteInfos.size(); i++) {
        info = Scene::TileSpriteInfos[i];
        frameStr = info.Sprite->Animations[info.AnimationIndex].Frames[info.FrameIndex];
        texture = info.Sprite->Spritesheets[frameStr.SheetNumber];
        srcStrides[i] = srcStride = texture->Width;
        tileSources[i] = (&((Uint32*)texture->Pixels)[frameStr.X + frameStr.Y * srcStride]);
        isPalettedSources[i] = Graphics::UsePalettes && texture->Paletted;
    }

    void (*pixelFunction)(Uint32*, Uint32*, int, int*, int*) = NULL;
    switch (blendFlag) {
        case BlendFlag_OPAQUE:
            pixelFunction = PixelNoFiltSetOpaque;
            break;
        case BlendFlag_TRANSPARENT:
            pixelFunction = PixelNoFiltSetTransparent;
            break;
        case BlendFlag_ADDITIVE:
            pixelFunction = PixelNoFiltSetAdditive;
            break;
        case BlendFlag_SUBTRACT:
            pixelFunction = PixelNoFiltSetSubtract;
            break;
        case BlendFlag_MATCH_EQUAL:
            pixelFunction = PixelNoFiltSetMatchEqual;
            break;
        case BlendFlag_MATCH_NOT_EQUAL:
            pixelFunction = PixelNoFiltSetMatchNotEqual;
            break;
        case BlendFlag_FILTER:
            pixelFunction = PixelNoFiltSetFilter;
            break;
    }

    TileScanLine* scanLine = &TileScanLineBuffer[dst_y1];
    for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) {
        dstPxLine = dstPx + dst_strideY;

        Sint64 srcX = scanLine->SrcX,
               srcY = scanLine->SrcY,
               srcDX = scanLine->DeltaX,
               srcDY = scanLine->DeltaY;

        Uint32 maxHorzCells = scanLine->MaxHorzCells;
        Uint32 maxVertCells = scanLine->MaxVertCells;

        void (*linePixelFunction)(Uint32*, Uint32*, int, int*, int*);

        int opacity = 0;
        if (Graphics::TextureBlend) {
            opacity = Alpha - (0xFF - scanLine->Opacity);
            if (opacity < 0)
                opacity = 0;
        }

        int* multTableAt = &MultTable[opacity << 8];
        int* multSubTableAt = &MultSubTable[opacity << 8];

        if (opacity == 0 && blendFlag == BlendFlag_TRANSPARENT)
            goto scanlineDone;

        linePixelFunction = pixelFunction;

        if (opacity != 0 && blendFlag == BlendFlag_OPAQUE)
            linePixelFunction = PixelNoFiltSetTransparent;
        if (opacity == 0xFF && blendFlag == BlendFlag_TRANSPARENT)
            linePixelFunction = PixelNoFiltSetOpaque;

        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0];

        for (int dst_x = dst_x1; dst_x < dst_x2; dst_x++) {
            int srcTX = srcX >> 16;
            int srcTY = srcY >> 16;

            sourceTileCellX = (srcX >> 20) & layerWidthTileMask;
            sourceTileCellY = (srcY >> 20) & layerHeightTileMask;

            if (maxHorzCells != 0)
                sourceTileCellX %= maxHorzCells;
            if (maxVertCells != 0)
                sourceTileCellY %= maxVertCells;

            tile = layer->Tiles[sourceTileCellX + (sourceTileCellY << layerWidthInBits)] & TILE_IDENT_MASK;
			// printf("tile: %X (%s), sourceTileCellX: %d, sourceTileCellY: %d\n", tile, layer->Name, sourceTileCellX, sourceTileCellY);
            if (tile != Scene::EmptyTile) {
                color = tileSources[tile][(srcTX & 15) + (srcTY & 15) * srcStrides[tile]];
                if (isPalettedSources[tile]) {
                    if (color)
                        linePixelFunction(&index[color], &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);
                }
                else {
                    if (color & 0xFF000000U)
                        linePixelFunction(&color, &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);
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
PUBLIC STATIC void     SoftwareRenderer::DrawSceneLayer(SceneLayer* layer, View* currentView) {
    if (layer->UsingCustomScanlineFunction && layer->DrawBehavior == DrawBehavior_CustomTileScanLines) {
        BytecodeObjectManager::Threads[0].RunFunction(&layer->CustomScanlineFunction, 0);
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

PUBLIC STATIC void     SoftwareRenderer::MakeFrameBufferID(ISprite* sprite, AnimFrame* frame) {
    frame->ID = 0;
}
