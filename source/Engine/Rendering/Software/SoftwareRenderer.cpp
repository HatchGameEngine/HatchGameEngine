#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/Texture.h>
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
    static Sint32            SpriteDeformBuffer[MAX_FRAMEBUFFER_HEIGHT];
    static bool              UseSpriteDeform;
};
#endif

#include <Engine/Rendering/Software/SoftwareRenderer.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/ResourceStream.h>

#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/BytecodeObjectManager.h>

GraphicsFunctions SoftwareRenderer::BackendFunctions;
Uint32            SoftwareRenderer::CompareColor = 0xFF000000U;
Sint32            SoftwareRenderer::CurrentPalette = -1;
Uint32            SoftwareRenderer::CurrentArrayBuffer = 0;
Uint32            SoftwareRenderer::PaletteColors[MAX_PALETTE_COUNT][0x100];
Uint8             SoftwareRenderer::PaletteIndexLines[MAX_FRAMEBUFFER_HEIGHT];
TileScanLine      SoftwareRenderer::TileScanLineBuffer[MAX_FRAMEBUFFER_HEIGHT];
Sint32            SoftwareRenderer::SpriteDeformBuffer[MAX_FRAMEBUFFER_HEIGHT];
bool              SoftwareRenderer::UseSpriteDeform = false;

int Alpha = 0xFF;
int BlendFlag = 0;

struct SWTextureData {
	Uint32*           Palette = NULL;
	int               PaletteSize = 0;
	int               PaletteCount = 0;
	int               TransparentColorIndex = 0;
};

inline Uint32 ColorAdd(Uint32 color1, Uint32 color2, int percent) {
	Uint32 r = (color1 & 0xFF0000U) + (((color2 & 0xFF0000U) * percent) >> 8);
	Uint32 g = (color1 & 0xFF00U) + (((color2 & 0xFF00U) * percent) >> 8);
	Uint32 b = (color1 & 0xFFU) + (((color2 & 0xFFU) * percent) >> 8);
    if (r > 0xFF0000U) r = 0xFF0000U;
	if (g > 0xFF00U) g = 0xFF00U;
	if (b > 0xFFU) b = 0xFFU;
	return r | g | b;
}
inline Uint32 ColorBlend(Uint32 color1, Uint32 color2, int percent) {
	Uint32 rb = color1 & 0xFF00FFU;
	Uint32 g  = color1 & 0x00FF00U;
	rb += (((color2 & 0xFF00FFU) - rb) * percent) >> 8;
	g  += (((color2 & 0x00FF00U) - g) * percent) >> 8;
	return (rb & 0xFF00FFU) | (g & 0x00FF00U);
}
inline Uint32 ColorSubtract(Uint32 color1, Uint32 color2, int percent) {
    Sint32 r = (color1 & 0xFF0000U) - (((color2 & 0xFF0000U) * percent) >> 8);
	Sint32 g = (color1 & 0xFF00U) - (((color2 & 0xFF00U) * percent) >> 8);
	Sint32 b = (color1 & 0xFFU) - (((color2 & 0xFFU) * percent) >> 8);
    if (r < 0) r = 0;
	if (g < 0) g = 0;
	if (b < 0) b = 0;
	return r | g | b;
}
inline int ColorMultiply(Uint32 color, Uint32 colorMult) {
    Uint32 R = (((colorMult >> 16) & 0xFF) + 1) * (color & 0xFF0000);
    Uint32 G = (((colorMult >> 8) & 0xFF) + 1) * (color & 0x00FF00);
    Uint32 B = (((colorMult) & 0xFF) + 1) * (color & 0x0000FF);
    return (int)((R >> 8) | (G >> 8) | (B >> 8));
}

inline void SetPixel(int x, int y, Uint32 color) {
	// if (DoDeform) x += Deform[y];

	// if (x <  Clip[0]) return;
	// if (y <  Clip[1]) return;
	// if (x >= Clip[2]) return;
	// if (y >= Clip[3]) return;

	// SetPixelFunction(x, y, GetPixelOutputColor(color));
	// SetPixelFunction(x, y, color);
}

inline Uint32 GetPixelOutputColor(Uint32 color) {
	// color = (this->*SetFilterFunction[0])(color);
	// color = (this->*SetFilterFunction[1])(color);
	// color = (this->*SetFilterFunction[2])(color);
	// color = (this->*SetFilterFunction[3])(color);

	return color;
}

bool SaveScreenshot(const char* filename) {
    /*
	Uint32* palette = (Uint32*)Memory::Calloc(256, sizeof(Uint32));
	Uint32* pixelChecked = (Uint32*)Memory::Calloc(FrameBufferSize, sizeof(Uint32));
	Uint32* pixelIndexes = (Uint32*)Memory::Calloc(FrameBufferSize, sizeof(Uint32));
	Uint32  paletteCount = 0;
	for (int checking = 0; checking < FrameBufferSize; checking++) {
		if (!pixelChecked[checking]) {
			Uint32 color = FrameBuffer[checking] & 0xFFFFFF;
			for (int comparing = checking; comparing < FrameBufferSize; comparing++) {
				if (color == (FrameBuffer[comparing] & 0xFFFFFF)) {
					pixelIndexes[comparing] = paletteCount;
					pixelChecked[comparing] = 1;
				}
			}
			if (paletteCount == 255) {
				Memory::Free(palette);
				Memory::Free(pixelChecked);
				Memory::Free(pixelIndexes);
				Log::Print(Log::LOG_ERROR, "Too many colors!");
				return false;
			}
			palette[paletteCount++] = color;
		}
	}

	char filenameStr[100];
	time_t now; time(&now);
	struct tm* local = localtime(&now);
	sprintf(filenameStr, "ImpostorEngine3_%02d-%02d-%02d_%02d-%02d-%02d.gif", local->tm_mday, local->tm_mon + 1, local->tm_year - 100, local->tm_hour, local->tm_min, local->tm_sec);

	GIF* gif = new GIF;
	gif->Colors = palette;
    gif->Data = pixelIndexes;
    gif->Width = Application::WIDTH;
    gif->Height = Application::HEIGHT;
    gif->TransparentColorIndex = 0xFF;
	if (!gif->Save(filenameStr)) {
		gif->Data = NULL;
		delete gif;
		Memory::Free(palette);
		Memory::Free(pixelChecked);
		Memory::Free(pixelIndexes);
		return false;
	}
	gif->Data = NULL;
	delete gif;
	Memory::Free(palette);
	Memory::Free(pixelChecked);
	Memory::Free(pixelIndexes);

    */

	return true;
}

PUBLIC STATIC inline Uint32 SoftwareRenderer::GetPixelIndex(ISprite* sprite, int x, int y) {
	// return sprite->Data[x + y * sprite->Width];
	return 0;
}

// Rendering functions
PUBLIC STATIC void    SoftwareRenderer::SetDrawAlpha(int a) {
	// DrawAlpha = a;
    //
	// if (SetPixelFunction != &SoftwareRenderer::SetPixelNormal && SetPixelFunction != &SoftwareRenderer::SetPixelAlpha) return;
    //
	// if (DrawAlpha == 0xFF)
	// 	SetPixelFunction = &SoftwareRenderer::SetPixelNormal;
	// else
	// 	SetPixelFunction = &SoftwareRenderer::SetPixelAlpha;
}
PUBLIC STATIC void    SoftwareRenderer::SetDrawFunc(int a) {
	// switch (a) {
	// 	case 1:
	// 		SetPixelFunction = &SoftwareRenderer::SetPixelAdditive;
	// 		break;
	// 	default:
	// 		SetPixelFunction = &SoftwareRenderer::SetPixelNormal;
	// 		SetDrawAlpha(DrawAlpha);
	// 		break;
	// }
}

PUBLIC STATIC void    SoftwareRenderer::SetFade(int fade) {
	// if (fade < 0)
	// 	Fade = 0x00;
	// else if (fade > 0xFF)
	// 	Fade = 0xFF;
	// else
	// 	Fade = fade;
}
PUBLIC STATIC void    SoftwareRenderer::SetFilter(int filter) {
	// DrawPixelFilter = filter;
    //
	// SetFilterFunction[0] = &SoftwareRenderer::FilterNone;
	// // SetFilterFunction[1] = &SoftwareRenderer::FilterNone;
	// // SetFilterFunction[2] = &SoftwareRenderer::FilterNone;
	// SetFilterFunction[3] = &SoftwareRenderer::FilterNone;
	// if ((DrawPixelFilter & 0x1) == 0x1)
	// 	SetFilterFunction[0] = &SoftwareRenderer::FilterGrayscale;
	// if ((DrawPixelFilter & 0x4) == 0x4)
	// 	SetFilterFunction[3] = &SoftwareRenderer::FilterFadeToBlack;
}
PUBLIC STATIC int     SoftwareRenderer::GetFilter() {
	// return DrawPixelFilter;
    return 0;
}

// Utility functions
PUBLIC STATIC bool    SoftwareRenderer::IsOnScreen(int x, int y, int w, int h) {
	// return (
	// 	x + w >= Clip[0] &&
	// 	y + h >= Clip[1] &&
	// 	x     <  Clip[2] &&
	// 	y     <  Clip[3]);
    return false;
}

// Drawing Shapes
PUBLIC STATIC void    SoftwareRenderer::DrawRectangleStroke(int x, int y, int width, int height, Uint32 color) {
	int x1, x2, y1, y2;

	x1 = x;
	x2 = x + width - 1;
	if (width < 0) {
		x1 += width; x2 -= width;
	}

	y1 = y;
	y2 = y + height - 1;
	if (height < 0) {
		y1 += height; y2 -= height;
	}

	for (int b = x1; b <= x2; b++) {
		SetPixel(b, y1, color);
		SetPixel(b, y2, color);
	}
	for (int b = y1 + 1; b < y2; b++) {
		SetPixel(x1, b, color);
		SetPixel(x2, b, color);
	}
}
PUBLIC STATIC void    SoftwareRenderer::DrawRectangleSkewedH(int x, int y, int width, int height, int sk, Uint32 color) {

}

PUBLIC STATIC void    SoftwareRenderer::DrawTextSprite(ISprite* sprite, int animation, char first, int x, int y, const char* string) {
	AnimFrame animframe;
	for (int i = 0; i < (int)strlen(string); i++) {
		animframe = sprite->Animations[animation].Frames[string[i] - first];
		// DrawSpriteNormal(sprite, animation, string[i] - first, x - animframe.OffsetX, y, false, false);

		x += animframe.Width;
	}
}
PUBLIC STATIC int     SoftwareRenderer::MeasureTextSprite(ISprite* sprite, int animation, char first, const char* string) {
	int x = 0;
	AnimFrame animframe;
	for (int i = 0; i < (int)strlen(string); i++) {
		animframe = sprite->Animations[animation].Frames[string[i] - first];
		x += animframe.Width;
	}
	return x;
}

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

// Utility Functions
int ColR = 0xFF;
int ColG = 0xFF;
int ColB = 0xFF;
Uint32 ColRGB = 0xFFFFFF;
int MultTable[0x10000];
int MultTableInv[0x10000];
int MultSubTable[0x10000];
int Sin0x200[0x200];
int Cos0x200[0x200];

int FilterCurrent[0x8000];

int FilterColor[0x8000];
int FilterInvert[0x8000];
int FilterBlackAndWhite[0x8000];
int* FilterTable = NULL;
// Initialization and disposal functions
PUBLIC STATIC void     SoftwareRenderer::Init() {
    SoftwareRenderer::BackendFunctions.Init();

    UseSpriteDeform = false;
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

    for (int a = 0; a < 0x200; a++) {
        float ang = a * M_PI / 0x100;
        Sin0x200[a] = (int)(Math::Sin(ang) * 0x200);
        Cos0x200[a] = (int)(Math::Cos(ang) * 0x200);
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
	// Memory::Free(Deform);
	// Memory::Free(FrameBuffer);
	// SDL_FreeSurface(WindowScreen);
	// SDL_DestroyTexture(ScreenTexture);
	// SDL_DestroyWindow(Window);
    //
	// Screen->pixels = ScreenOriginalPixels;
	// SDL_FreeSurface(Screen);
    SoftwareRenderer::BackendFunctions.Dispose();
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

enum BlendFlags {
    BlendFlag_OPAQUE = 0,
    BlendFlag_TRANSPARENT,
    BlendFlag_ADDITIVE,
    BlendFlag_SUBTRACT,
    BlendFlag_MATCH_EQUAL,
    BlendFlag_MATCH_NOT_EQUAL,
    BlendFlag_FILTER,
};

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

PUBLIC STATIC void     SoftwareRenderer::Resize(int width, int height) {
    // Deform = (Sint8*)Memory::Realloc(Deform, height);
    // // memset(Deform + last_height, 0, height - last_height);
    //
    // if (!ClipSet) {
    //     Clip[0] = 0;
    // 	Clip[1] = 0;
    // 	Clip[2] = width;
    // 	Clip[3] = height;
    // }
    //
	// FrameBuffer = (Uint32*)Memory::Realloc(FrameBuffer, width * height * sizeof(Uint32));
    // if (width * height - FrameBufferSize > 0)
    //     memset(FrameBuffer + FrameBufferSize, 0, width * height - FrameBufferSize);
    // FrameBufferSize = width * height;
	// FrameBufferStride = width;
}

PUBLIC STATIC void     SoftwareRenderer::SetClip(float x, float y, float width, float height) {
    // ClipSet = true;
    //
    // Clip[0] = (int)x;
	// Clip[1] = (int)y;
	// Clip[2] = (int)x + (int)w;
	// Clip[3] = (int)y + (int)h;
    //
	// if (Clip[0] < 0)
	// 	Clip[0] = 0;
	// if (Clip[1] < 0)
	// 	Clip[1] = 0;
	// if (Clip[2] > Application::WIDTH)
	// 	Clip[2] = Application::WIDTH;
	// if (Clip[3] > Application::HEIGHT)
	// 	Clip[3] = Application::HEIGHT;
}
PUBLIC STATIC void     SoftwareRenderer::ClearClip() {
    // ClipSet = false;
    //
    // Clip[0] = 0;
	// Clip[1] = 0;
	// Clip[2] = Application::WIDTH;
	// Clip[3] = Application::HEIGHT;
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

#define SRC_CHECK false
#define GET_CLIP_BOUNDS(x1, y1, x2, y2) \
    if (Graphics::CurrentClip.Enabled) { \
        x1 = Graphics::CurrentClip.X; \
        y1 = Graphics::CurrentClip.Y; \
        x2 = Graphics::CurrentClip.X + Graphics::CurrentClip.Width; \
        y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height; \
    } \
    else { \
        x1 = 0; \
        y1 = 0; \
        x2 = (int)Graphics::CurrentRenderTarget->Width; \
        y2 = (int)Graphics::CurrentRenderTarget->Height; \
    }
#define ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity) \
    if (FilterTable != &FilterColor[0]) \
        blendFlag = BlendFlag_FILTER; \
    if (opacity == 0 && (blendFlag != BlendFlag_OPAQUE && blendFlag != BlendFlag_FILTER)) \
        return; \
    if (opacity != 0 && blendFlag == BlendFlag_OPAQUE) \
        blendFlag = BlendFlag_TRANSPARENT; \
    if (opacity == 0xFF && blendFlag == BlendFlag_TRANSPARENT) \
        blendFlag = BlendFlag_OPAQUE;

// Filterless versions
#define GET_R(color) ((color >> 16) & 0xFF)
#define GET_G(color) ((color >> 8) & 0xFF)
#define GET_B(color) ((color) & 0xFF)
#define ISOLATE_R(color) (color & 0xFF0000)
#define ISOLATE_G(color) (color & 0x00FF00)
#define ISOLATE_B(color) (color & 0x0000FF)

inline void PixelNoFiltSetOpaque(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    *dst = *src;
}
inline void PixelNoFiltSetTransparent(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    int* multInvTableAt = &MultTableInv[opacity << 8];
    *dst = 0xFF000000U
        | (multTableAt[GET_R(*src)] + multInvTableAt[GET_R(*dst)]) << 16
        | (multTableAt[GET_G(*src)] + multInvTableAt[GET_G(*dst)]) << 8
        | (multTableAt[GET_B(*src)] + multInvTableAt[GET_B(*dst)]);
}
inline void PixelNoFiltSetAdditive(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    Uint32 R = (multTableAt[GET_R(*src)] << 16) + ISOLATE_R(*dst);
    Uint32 G = (multTableAt[GET_G(*src)] << 8) + ISOLATE_G(*dst);
    Uint32 B = (multTableAt[GET_B(*src)]) + ISOLATE_B(*dst);
    if (R > 0xFF0000) R = 0xFF0000;
    if (G > 0x00FF00) G = 0x00FF00;
    if (B > 0x0000FF) B = 0x0000FF;
    *dst = 0xFF000000U | R | G | B;
}
inline void PixelNoFiltSetSubtract(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    Sint32 R = (multSubTableAt[GET_R(*src)] << 16) + ISOLATE_R(*dst);
    Sint32 G = (multSubTableAt[GET_G(*src)] << 8) + ISOLATE_G(*dst);
    Sint32 B = (multSubTableAt[GET_B(*src)]) + ISOLATE_B(*dst);
    if (R < 0) R = 0;
    if (G < 0) G = 0;
    if (B < 0) B = 0;
    *dst = 0xFF000000U | R | G | B;
}
inline void PixelNoFiltSetMatchEqual(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    // if (*dst == SoftwareRenderer::CompareColor)
        // *dst = *src;
    if ((*dst & 0xFCFCFC) == (SoftwareRenderer::CompareColor & 0xFCFCFC))
        *dst = *src;
}
inline void PixelNoFiltSetMatchNotEqual(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    // if (*dst != SoftwareRenderer::CompareColor)
        // *dst = *src;
    if ((*dst & 0xFCFCFC) != (SoftwareRenderer::CompareColor & 0xFCFCFC))
        *dst = *src;
}
inline void PixelNoFiltSetFilter(Uint32* src, Uint32* dst, int opacity, int* multTableAt, int* multSubTableAt) {
    Uint32 col = *dst;
    *dst = FilterTable[(col & 0xF80000) >> 9 | (col & 0xF800) >> 6 | (col & 0xF8) >> 3];
}

struct Contour {
    int MinX;
    int MinR;
    int MinG;
    int MinB;
    int MinU;
    int MinV;

    int MaxX;
    int MaxR;
    int MaxG;
    int MaxB;
    int MaxU;
    int MaxV;
};
Contour ContourField[MAX_FRAMEBUFFER_HEIGHT];
void DrawPolygonBlendUVScanLine(int color1, int color2, Vector2 uv1, Vector2 uv2, int x1, int y1, int x2, int y2) {
    int xStart = x1 >> 16;
    int xEnd   = x2 >> 16;
    int yStart = y1 >> 16;
    int yEnd   = y2 >> 16;
    int cStart = color1;
    int cEnd   = color2;
    if (yStart == yEnd)
        return;

    // swap
    if (yStart > yEnd) {
        xStart = x2 >> 16;
        xEnd   = x1 >> 16;
        yStart = y2 >> 16;
        yEnd   = y1 >> 16;
        cStart = color2;
        cEnd   = color1;
    }

    int minX, minY, maxX, maxY;
    GET_CLIP_BOUNDS(minX, minY, maxX, maxY);

    int yEndBound = yEnd + 1;
    if (yEndBound < minY || yStart >= maxY)
        return;

    if (yEndBound > maxY)
        yEndBound = maxY;

    int colorBegRED = (cStart & 0xFF0000);
    int colorEndRED = (cEnd & 0xFF0000);
    int colorBegGREEN = (cStart & 0xFF00) << 8;
    int colorEndGREEN = (cEnd & 0xFF00) << 8;
    int colorBegBLUE = (cStart & 0xFF) << 16;
    int colorEndBLUE = (cEnd & 0xFF) << 16;

    int linePointSubpxX = xStart << 16;
    int yDiff = (yEnd - yStart);
    int dx = ((xEnd - xStart) << 16) / yDiff;
    int dxRED = 0, dxGREEN = 0, dxBLUE = 0;

    if (colorBegRED != colorEndRED)
        dxRED = (colorEndRED - colorBegRED) / yDiff;
    if (colorBegGREEN != colorEndGREEN)
        dxGREEN = (colorEndGREEN - colorBegGREEN) / yDiff;
    if (colorBegBLUE != colorEndBLUE)
        dxBLUE = (colorEndBLUE - colorBegBLUE) / yDiff;

    if (yStart < 0) {
        linePointSubpxX -= yStart * dx;
        colorBegRED -= yStart * dxRED;
        colorBegGREEN -= yStart * dxGREEN;
        colorBegBLUE -= yStart * dxBLUE;
        yStart = 0;
    }

    Contour* contour = &ContourField[yStart];
    if (yStart < yEndBound) {
        int lineHeight = yEndBound - yStart;
        while (lineHeight--) {
            int linePointX = linePointSubpxX >> 16;

            if (linePointX <= minX) {
                contour->MinX = minX;
                contour->MinR = colorBegRED;
                contour->MinG = colorBegGREEN;
                contour->MinB = colorBegBLUE;
            }
            else if (linePointX >= maxX) {
                contour->MaxX = maxX;
                contour->MaxR = colorBegRED;
                contour->MaxG = colorBegGREEN;
                contour->MaxB = colorBegBLUE;
            }
            else {
                if (linePointX < contour->MinX) {
                    contour->MinX = linePointX;
                    contour->MinR = colorBegRED;
                    contour->MinG = colorBegGREEN;
                    contour->MinB = colorBegBLUE;
                }
    			if (linePointX > contour->MaxX) {
                    contour->MaxX = linePointX;
                    contour->MaxR = colorBegRED;
                    contour->MaxG = colorBegGREEN;
                    contour->MaxB = colorBegBLUE;
                }
            }

            linePointSubpxX += dx;
            colorBegRED += dxRED;
            colorBegGREEN += dxGREEN;
            colorBegBLUE += dxBLUE;
            contour++;
    	}
    }
}
void DrawPolygonBlendUV(Texture* texture, Vector2* positions, Vector2* uvs, int* colors, int count, int opacity, int blendFlag) {
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

    int scanLineCount = dst_y2 - dst_y1;
    Contour* contourPtr = &ContourField[dst_y1];
    while (scanLineCount--) {
        contourPtr->MinX = INT_MAX;
        contourPtr->MaxX = INT_MIN;
        contourPtr++;
    }

    int*     lastColor = colors;
    Vector2* lastPosition = positions;
    Vector2* lastUV = uvs;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            DrawPolygonBlendUVScanLine(lastColor[0], lastColor[1], lastUV[0], lastUV[1], lastPosition[0].X, lastPosition[0].Y, lastPosition[1].X, lastPosition[1].Y);
            lastPosition++;
            lastColor++;
            lastUV++;
        }
    }

    DrawPolygonBlendUVScanLine(lastColor[0], colors[0], lastUV[0], uvs[0], lastPosition[0].X, lastPosition[0].Y, positions[0].X, positions[0].Y);

    Sint32 col, colR, colG, colB, colU, colV, dxR, dxG, dxB, dxU, dxV, contLen;

    #define DRAW_POLYGONBLENDUV(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        Contour contour = ContourField[dst_y]; \
        contLen = contour.MaxX - contour.MinX; \
        if (contLen <= 0) { \
            dst_strideY += dstStride; \
            continue; \
        } \
        colR = contour.MinR; \
        colG = contour.MinG; \
        colB = contour.MinB; \
        colU = contour.MinU; \
        colV = contour.MinV; \
        dxR = (contour.MaxR - colR) / contLen; \
        dxG = (contour.MaxG - colG) / contLen; \
        dxB = (contour.MaxB - colB) / contLen; \
        dxU = (contour.MaxU - colU) / contLen; \
        dxV = (contour.MaxV - colV) / contLen; \
        for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
            col = ((colR) & 0xFF0000) | ((colG >> 8) & 0xFF00) | ((colB >> 16) & 0xFF); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            colR += dxR; \
            colG += dxG; \
            colB += dxB; \
        } \
        dst_strideY += dstStride; \
    }

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;
    switch (blendFlag) {
        case 0:
            DRAW_POLYGONBLENDUV(PixelNoFiltSetOpaque);
            break;
        case 1:
            DRAW_POLYGONBLENDUV(PixelNoFiltSetTransparent);
            break;
        case 2:
            DRAW_POLYGONBLENDUV(PixelNoFiltSetAdditive);
            break;
        case 3:
            DRAW_POLYGONBLENDUV(PixelNoFiltSetSubtract);
            break;
        case 4:
            DRAW_POLYGONBLENDUV(PixelNoFiltSetMatchEqual);
            break;
        case 5:
            DRAW_POLYGONBLENDUV(PixelNoFiltSetMatchNotEqual);
            break;
        case 6:
            DRAW_POLYGONBLENDUV(PixelNoFiltSetFilter);
            break;
    }

    #undef DRAW_POLYGONBLENDUV
}
void DrawPolygonBlendScanLine(int color1, int color2, int x1, int y1, int x2, int y2) {
    int xStart = x1 >> 16;
    int xEnd   = x2 >> 16;
    int yStart = y1 >> 16;
    int yEnd   = y2 >> 16;
    int cStart = color1;
    int cEnd   = color2;
    if (yStart == yEnd)
        return;

    // swap
    if (yStart > yEnd) {
        xStart = x2 >> 16;
        xEnd   = x1 >> 16;
        yStart = y2 >> 16;
        yEnd   = y1 >> 16;
        cStart = color2;
        cEnd   = color1;
    }

    int minX, minY, maxX, maxY;
    GET_CLIP_BOUNDS(minX, minY, maxX, maxY);

    int yEndBound = yEnd + 1;
    if (yEndBound < minY || yStart >= maxY)
        return;

    if (yEndBound > maxY)
        yEndBound = maxY;

    int colorBegRED = (cStart & 0xFF0000);
    int colorEndRED = (cEnd & 0xFF0000);
    int colorBegGREEN = (cStart & 0xFF00) << 8;
    int colorEndGREEN = (cEnd & 0xFF00) << 8;
    int colorBegBLUE = (cStart & 0xFF) << 16;
    int colorEndBLUE = (cEnd & 0xFF) << 16;

    int linePointSubpxX = xStart << 16;
    int yDiff = (yEnd - yStart);
    int dx = ((xEnd - xStart) << 16) / yDiff;
    int dxRED = 0, dxGREEN = 0, dxBLUE = 0;

    if (colorBegRED != colorEndRED)
        dxRED = (colorEndRED - colorBegRED) / yDiff;
    if (colorBegGREEN != colorEndGREEN)
        dxGREEN = (colorEndGREEN - colorBegGREEN) / yDiff;
    if (colorBegBLUE != colorEndBLUE)
        dxBLUE = (colorEndBLUE - colorBegBLUE) / yDiff;

    if (yStart < 0) {
        linePointSubpxX -= yStart * dx;
        colorBegRED -= yStart * dxRED;
        colorBegGREEN -= yStart * dxGREEN;
        colorBegBLUE -= yStart * dxBLUE;
        yStart = 0;
    }

    Contour* contour = &ContourField[yStart];
    if (yStart < yEndBound) {
        int lineHeight = yEndBound - yStart;
        while (lineHeight--) {
            int linePointX = linePointSubpxX >> 16;

            if (linePointX <= minX) {
                contour->MinX = minX;
                contour->MinR = colorBegRED;
                contour->MinG = colorBegGREEN;
                contour->MinB = colorBegBLUE;
            }
            else if (linePointX >= maxX) {
                contour->MaxX = maxX;
                contour->MaxR = colorBegRED;
                contour->MaxG = colorBegGREEN;
                contour->MaxB = colorBegBLUE;
            }
            else {
                if (linePointX < contour->MinX) {
                    contour->MinX = linePointX;
                    contour->MinR = colorBegRED;
                    contour->MinG = colorBegGREEN;
                    contour->MinB = colorBegBLUE;
                }
    			if (linePointX > contour->MaxX) {
                    contour->MaxX = linePointX;
                    contour->MaxR = colorBegRED;
                    contour->MaxG = colorBegGREEN;
                    contour->MaxB = colorBegBLUE;
                }
            }

            linePointSubpxX += dx;
            colorBegRED += dxRED;
            colorBegGREEN += dxGREEN;
            colorBegBLUE += dxBLUE;
            contour++;
    	}
    }
}
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

    int scanLineCount = dst_y2 - dst_y1;
    Contour* contourPtr = &ContourField[dst_y1];
    while (scanLineCount--) {
        contourPtr->MinX = INT_MAX;
        contourPtr->MaxX = INT_MIN;
        contourPtr++;
    }

    int* lastColor = colors;
    Vector2* lastVector = positions;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            DrawPolygonBlendScanLine(lastColor[0], lastColor[1], lastVector[0].X, lastVector[0].Y, lastVector[1].X, lastVector[1].Y);
            lastVector++;
            lastColor++;
        }
    }

    DrawPolygonBlendScanLine(lastColor[0], colors[0], lastVector[0].X, lastVector[0].Y, positions[0].X, positions[0].Y);

    Sint32 col, colR, colG, colB, dxR, dxG, dxB, contLen;

    #define DRAW_POLYGONBLEND(pixelFunction) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        Contour contour = ContourField[dst_y]; \
        contLen = contour.MaxX - contour.MinX; \
        if (contLen <= 0) { \
            dst_strideY += dstStride; \
            continue; \
        } \
        colR = contour.MinR; \
        colG = contour.MinG; \
        colB = contour.MinB; \
        dxR = (contour.MaxR - colR) / contLen; \
        dxG = (contour.MaxG - colG) / contLen; \
        dxB = (contour.MaxB - colB) / contLen; \
        for (int dst_x = contour.MinX; dst_x < contour.MaxX; dst_x++) { \
            col = ((colR) & 0xFF0000) | ((colG >> 8) & 0xFF00) | ((colB >> 16) & 0xFF); \
            pixelFunction((Uint32*)&col, &dstPx[dst_x + dst_strideY], opacity, multTableAt, multSubTableAt); \
            colR += dxR; \
            colG += dxG; \
            colB += dxB; \
        } \
        dst_strideY += dstStride; \
    }

    int* multTableAt = &MultTable[opacity << 8];
    int* multSubTableAt = &MultSubTable[opacity << 8];
    int dst_strideY = dst_y1 * dstStride;
    switch (blendFlag) {
        case 0:
            DRAW_POLYGONBLEND(PixelNoFiltSetOpaque);
            break;
        case 1:
            DRAW_POLYGONBLEND(PixelNoFiltSetTransparent);
            break;
        case 2:
            DRAW_POLYGONBLEND(PixelNoFiltSetAdditive);
            break;
        case 3:
            DRAW_POLYGONBLEND(PixelNoFiltSetSubtract);
            break;
        case 4:
            DRAW_POLYGONBLEND(PixelNoFiltSetMatchEqual);
            break;
        case 5:
            DRAW_POLYGONBLEND(PixelNoFiltSetMatchNotEqual);
            break;
        case 6:
            DRAW_POLYGONBLEND(PixelNoFiltSetFilter);
            break;
    }

    #undef DRAW_POLYGONBLEND
}
void DrawPolygonScanLine(int x1, int y1, int x2, int y2) {
    int xStart = x1 >> 16;
    int xEnd   = x2 >> 16;
    int yStart = y1 >> 16;
    int yEnd   = y2 >> 16;
    if (yStart == yEnd)
        return;

    // swap
    if (yStart > yEnd) {
        xStart = x2 >> 16;
        xEnd   = x1 >> 16;
        yStart = y2 >> 16;
        yEnd   = y1 >> 16;
    }

    int minX, minY, maxX, maxY;
    GET_CLIP_BOUNDS(minX, minY, maxX, maxY);

    int yEndBound = yEnd + 1;
    if (yEndBound < minY || yStart >= maxY)
        return;

    if (yEndBound > maxY)
        yEndBound = maxY;

    int linePointSubpxX = xStart << 16;
    int dx = ((xEnd - xStart) << 16) / (yEnd - yStart);

    if (yStart < 0) {
        linePointSubpxX -= yStart * dx;
        yStart = 0;
    }

    Contour* contour = &ContourField[yStart];
    if (yStart < yEndBound) {
        int lineHeight = yEndBound - yStart;
        while (lineHeight--) {
            int linePointX = linePointSubpxX >> 16;

            if (linePointX <= minX)
                contour->MinX = minX;
            else if (linePointX >= maxX)
                contour->MaxX = maxX;
            else {
                if (linePointX < contour->MinX)
                    contour->MinX = linePointX;
    			if (linePointX > contour->MaxX)
                    contour->MaxX = linePointX;
            }

            linePointSubpxX += dx;
            contour++;
    	}
    }
}
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

    int scanLineCount = dst_y2 - dst_y1;
    Contour* contourPtr = &ContourField[dst_y1];
    while (scanLineCount--) {
        contourPtr->MinX = INT_MAX;
        contourPtr->MaxX = INT_MIN;
        contourPtr++;
    }

    Vector2* lastVector = positions;
    if (count > 1) {
        int countRem = count - 1;
        while (countRem--) {
            DrawPolygonScanLine(lastVector[0].X, lastVector[0].Y, lastVector[1].X, lastVector[1].Y);
            lastVector++;
        }
    }
    DrawPolygonScanLine(lastVector[0].X, lastVector[0].Y, positions[0].X, positions[0].Y);

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
        case 0:
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
        case 1:
            DRAW_POLYGON(PixelNoFiltSetTransparent);
            break;
        case 2:
            DRAW_POLYGON(PixelNoFiltSetAdditive);
            break;
        case 3:
            DRAW_POLYGON(PixelNoFiltSetSubtract);
            break;
        case 4:
            DRAW_POLYGON(PixelNoFiltSetMatchEqual);
            break;
        case 5:
            DRAW_POLYGON(PixelNoFiltSetMatchNotEqual);
            break;
        case 6:
            DRAW_POLYGON(PixelNoFiltSetFilter);
            break;
    }

    #undef DRAW_POLYGON
}

// Drawing 3D
ArrayBuffer ArrayBuffers[MAX_ARRAY_BUFFERS];
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_Init(Uint32 arrayBufferIndex, Uint32 maxVertices) {
    if (arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;

    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->VertexCapacity) {
        arrayBuffer->VertexCapacity = maxVertices;

        arrayBuffer->VertexBuffer = (VertexAttribute*)Memory::Calloc(maxVertices, sizeof(VertexAttribute));
        arrayBuffer->FaceSizeBuffer = (Uint8*)Memory::Calloc(maxVertices, sizeof(Uint8));
        arrayBuffer->FaceInfoBuffer = (FaceInfo*)Memory::Calloc((maxVertices >> 1), sizeof(FaceInfo));

        arrayBuffer->PerspectiveBitshiftX = 8;
        arrayBuffer->PerspectiveBitshiftY = 8;
    }
    else {
        arrayBuffer->VertexCapacity = maxVertices;

        arrayBuffer->VertexBuffer = (VertexAttribute*)Memory::Realloc(arrayBuffer->VertexBuffer, maxVertices * sizeof(VertexAttribute));
        arrayBuffer->FaceSizeBuffer = (Uint8*)Memory::Realloc(arrayBuffer->FaceSizeBuffer, maxVertices * sizeof(Uint8));
        arrayBuffer->FaceInfoBuffer = (FaceInfo*)Memory::Realloc(arrayBuffer->FaceInfoBuffer, (maxVertices >> 1) * sizeof(FaceInfo));

        arrayBuffer->PerspectiveBitshiftX = 8;
        arrayBuffer->PerspectiveBitshiftY = 8;
    }

    arrayBuffer->Initialized = true;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_SetAmbientLighting(Uint32 arrayBufferIndex, Uint32 r, Uint32 g, Uint32 b) {
    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->LightingAmbientR = r;
    arrayBuffer->LightingAmbientG = g;
    arrayBuffer->LightingAmbientB = b;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_SetDiffuseLighting(Uint32 arrayBufferIndex, Uint32 r, Uint32 g, Uint32 b) {
    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->LightingDiffuseR = r;
    arrayBuffer->LightingDiffuseG = g;
    arrayBuffer->LightingDiffuseB = b;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_SetSpecularLighting(Uint32 arrayBufferIndex, Uint32 r, Uint32 g, Uint32 b) {
    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->LightingSpecularR = r;
    arrayBuffer->LightingSpecularG = g;
    arrayBuffer->LightingSpecularB = b;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_DrawBegin(Uint32 arrayBufferIndex) {
    if (arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;

    ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->Initialized)
        return;

    arrayBuffer->VertexCount = 0;
    arrayBuffer->FaceCount = 0;

    // arrayBuffer->LightingAmbientR = 160;
    // arrayBuffer->LightingAmbientG = 160;
    // arrayBuffer->LightingAmbientB = 160;
    // arrayBuffer->LightingDiffuseR = 8;
    // arrayBuffer->LightingDiffuseG = 8;
    // arrayBuffer->LightingDiffuseB = 8;
    // arrayBuffer->LightingSpecularR = 14;
    // arrayBuffer->LightingSpecularG = 14;
    // arrayBuffer->LightingSpecularB = 14;
}
PUBLIC STATIC void     SoftwareRenderer::ArrayBuffer_DrawFinish(Uint32 arrayBufferIndex, Uint32 drawMode) {
    if (arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return;

    int facesRemaining;
    Uint8* faceSizePtr;
    FaceInfo* faceInfoPtr;
    ArrayBuffer* arrayBuffer;
    Uint32 bitshiftX, bitshiftY;
    VertexAttribute* vertexAttribsPtr;
    FaceInfo temp, *faceInfoPtrA, *faceInfoPtrB, *faceInfoPtrTop;

    View* currentView = &Scene::Views[Scene::ViewCurrent];
    int cx = (int)std::floor(currentView->X);
    int cy = (int)std::floor(currentView->Y);

    Matrix4x4* out = Graphics::ModelViewMatrix.top();
    int x = out->Values[12];
    int y = out->Values[13];
    x -= cx;
    y -= cy;

    int blendFlag = BlendFlag;
    int opacity = Alpha;

    ALTER_BLENDFLAG_AND_OPACITY(blendFlag, opacity);

    arrayBuffer = &ArrayBuffers[arrayBufferIndex];
    if (!arrayBuffer->Initialized)
        return;

    vertexAttribsPtr = arrayBuffer->VertexBuffer; // R
    faceInfoPtr = arrayBuffer->FaceInfoBuffer; // RW
    faceSizePtr = arrayBuffer->FaceSizeBuffer; // R
    bitshiftX = arrayBuffer->PerspectiveBitshiftX;
    bitshiftY = arrayBuffer->PerspectiveBitshiftY;

    // Get the face depth and vertices' start index
    int verticesStartIndex = 0;
    for (int f = 0; f < arrayBuffer->FaceCount; f++) {
        switch (*faceSizePtr) {
            case 2:
                // (50%, 50%)
                // faceInfoPtr->Depth  = vertexAttribsPtr[0].Position.Z >> 1;
                // faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z >> 1;
				faceInfoPtr->Depth  = vertexAttribsPtr[0].Position.Z;
				faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z;
				faceInfoPtr->Depth /= 2;
                vertexAttribsPtr += 2;
                break;
            case 3:
                // (25%, 25%, 50%)
                // faceInfoPtr->Depth  = vertexAttribsPtr[0].Position.Z >> 2;
                // faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z >> 2;
                // faceInfoPtr->Depth += vertexAttribsPtr[2].Position.Z >> 1;
				faceInfoPtr->Depth  = vertexAttribsPtr[0].Position.Z;
				faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z;
				faceInfoPtr->Depth += vertexAttribsPtr[2].Position.Z;
				faceInfoPtr->Depth /= 3;
                vertexAttribsPtr += 3;
                break;
            case 4:
                // (25%, 25%, 25%, 25%)
                // faceInfoPtr->Depth  = vertexAttribsPtr[0].Position.Z >> 2;
                // faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z >> 2;
                // faceInfoPtr->Depth += vertexAttribsPtr[2].Position.Z >> 2;
                // faceInfoPtr->Depth += vertexAttribsPtr[3].Position.Z >> 2;
				faceInfoPtr->Depth  = vertexAttribsPtr[0].Position.Z;
				faceInfoPtr->Depth += vertexAttribsPtr[1].Position.Z;
				faceInfoPtr->Depth += vertexAttribsPtr[2].Position.Z;
				faceInfoPtr->Depth += vertexAttribsPtr[3].Position.Z;
				faceInfoPtr->Depth /= 4;
                vertexAttribsPtr += 4;
                break;
            default:
                faceInfoPtr->Depth  = vertexAttribsPtr[0].Position.Z;
                vertexAttribsPtr += *faceSizePtr;
                break;
        }

        faceInfoPtr->VerticesStartIndex = verticesStartIndex;
        verticesStartIndex += *faceSizePtr;

        faceInfoPtr++;
        faceSizePtr++;
    }

    // Sort face infos by depth
	if (arrayBuffer->FaceCount > 1) {
		facesRemaining = arrayBuffer->FaceCount - 1;
		faceInfoPtrTop = arrayBuffer->FaceInfoBuffer;
		faceInfoPtrA = faceInfoPtrTop + 1;
		while (facesRemaining--) {
			temp = *faceInfoPtrA;
			faceInfoPtrB = faceInfoPtrA - 1;
			while (faceInfoPtrB >= faceInfoPtrTop && faceInfoPtrB->Depth < temp.Depth) {
				faceInfoPtrB[1] = faceInfoPtrB[0];
				faceInfoPtrB--;
			}
			faceInfoPtrB[1] = temp;
			faceInfoPtrA++;
		}
	}

    #define CLAMP_VAL(v, a, b) if (v < a) v = a; else if (v > b) v = b;

    // sas
    VertexAttribute *vertex, *vertexFirst;
    faceInfoPtr = arrayBuffer->FaceInfoBuffer;
    faceSizePtr = arrayBuffer->FaceSizeBuffer;

    int widthHalfSubpx = (int)(currentView->Width / 2) << 16;
    int heightHalfSubpx = (int)(currentView->Height / 2) << 16;
    switch (drawMode & 7) {
        // Lines, Solid Colored
        case 0:
            for (int f = 0; f < arrayBuffer->FaceCount; f++) {
                int vertexCountPerFaceMinus1 = *faceSizePtr - 1;
                vertexFirst = &arrayBuffer->VertexBuffer[faceInfoPtr->VerticesStartIndex];
                vertex = vertexFirst;

                if (drawMode & 8) {
                    widthHalfSubpx >>= 16;
                    heightHalfSubpx >>= 16;
                    #define FIX_X(x) (((x << bitshiftX) / vertexZ) - cx + widthHalfSubpx)
                    #define FIX_Y(x) (((y << bitshiftY) / vertexZ) - cy + heightHalfSubpx)
                    while (vertexCountPerFaceMinus1--) {
                        int vertexZ = vertex->Position.Z;
                        if (vertexZ < 0x100)
                            goto mrt_line_solid_NEXT_FACE;

                        ColRGB = vertex->Color;
                        SoftwareRenderer::StrokeLine(FIX_X(vertex[0].Position.X), FIX_Y(vertex[0].Position.Y), FIX_X(vertex[1].Position.X), FIX_Y(vertex[1].Position.Y));
                        vertex++;
                    }
                    int vertexZ = vertex->Position.Z;
                    if (vertexZ < 0x100)
                        goto mrt_line_solid_NEXT_FACE;
                    ColRGB = vertex->Color;
                    SoftwareRenderer::StrokeLine(FIX_X(vertex->Position.X), FIX_Y(vertex->Position.Y), FIX_X(vertexFirst->Position.X), FIX_Y(vertexFirst->Position.Y));
                    #undef FIX_X
                    #undef FIX_Y
                }
                else {
                    while (vertexCountPerFaceMinus1--) {
                        ColRGB = vertex->Color;
                        SoftwareRenderer::StrokeLine(vertex[0].Position.X >> 8, vertex[0].Position.Y >> 8, vertex[1].Position.X >> 8, vertex[1].Position.Y >> 8);
                        vertex++;
                    }
                    ColRGB = vertex->Color;
                    SoftwareRenderer::StrokeLine(vertex->Position.X >> 8, vertex->Position.Y >> 8, vertexFirst->Position.X >> 8, vertexFirst->Position.Y >> 8);
                }

                mrt_line_solid_NEXT_FACE:
                faceSizePtr++;
                faceInfoPtr++;
            }
            break;
        // Lines, Flat Shading
        case 2:
        // Lines, Smooth Shading
        case 4:
            for (int f = 0; f < arrayBuffer->FaceCount; f++) {
                int vertexCountPerFaceMinus1 = *faceSizePtr - 1;
                vertexFirst = &arrayBuffer->VertexBuffer[faceInfoPtr->VerticesStartIndex];
                vertex = vertexFirst;

                int averageNormalY = vertex[0].Normal.Y;
                switch (*faceSizePtr) {
                    case 2:
                        averageNormalY += vertex[1].Normal.Y;
                        break;
                    case 3:
                        averageNormalY += vertex[1].Normal.Y + vertex[2].Normal.Y;
                        break;
                    case 4:
                        averageNormalY += vertex[1].Normal.Y + vertex[2].Normal.Y + vertex[3].Normal.Y;
                        break;
                }
                averageNormalY /= *faceSizePtr;

                int color = vertex->Color;
                int col_r = (color >> 16) & 0xFF;
                int col_g = (color >> 8) & 0xFF;
                int col_b = (color) & 0xFF;
                int specularR = 0, specularG = 0, specularB = 0;

                int ambientNormalY = averageNormalY >> 10;
                int reweightedNormal = (averageNormalY >> 2) * (abs(averageNormalY) >> 2);

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

                // color
                color = col_r << 16 | col_g << 8 | col_b;

                ColRGB = color;
                if (drawMode & 8) {
                    widthHalfSubpx >>= 16;
                    heightHalfSubpx >>= 16;
                    #define FIX_X(x) (((x << bitshiftX) / vertexZ) - cx + widthHalfSubpx)
                    #define FIX_Y(x) (((y << bitshiftY) / vertexZ) - cy + heightHalfSubpx)
                    while (vertexCountPerFaceMinus1--) {
                        int vertexZ = vertex->Position.Z;
                        if (vertexZ < 0x100)
                            goto mrt_line_smooth_NEXT_FACE;

                        SoftwareRenderer::StrokeLine(FIX_X(vertex[0].Position.X), FIX_Y(vertex[0].Position.Y), FIX_X(vertex[1].Position.X), FIX_Y(vertex[1].Position.Y));
                        vertex++;
                    }
                    int vertexZ = vertex->Position.Z;
                    if (vertexZ < 0x100)
                        goto mrt_line_smooth_NEXT_FACE;
                    SoftwareRenderer::StrokeLine(FIX_X(vertex->Position.X), FIX_Y(vertex->Position.Y), FIX_X(vertexFirst->Position.X), FIX_Y(vertexFirst->Position.Y));
                    #undef FIX_X
                    #undef FIX_Y
                }
                else {
                    while (vertexCountPerFaceMinus1--) {
                        SoftwareRenderer::StrokeLine(vertex[0].Position.X >> 8, vertex[0].Position.Y >> 8, vertex[1].Position.X >> 8, vertex[1].Position.Y >> 8);
                        vertex++;
                    }
                    SoftwareRenderer::StrokeLine(vertex->Position.X >> 8, vertex->Position.Y >> 8, vertexFirst->Position.X >> 8, vertexFirst->Position.Y >> 8);
                }

                mrt_line_smooth_NEXT_FACE:
                faceSizePtr++;
                faceInfoPtr++;
            }
            break;
        // Polygons, Solid Colored
        case 1:
            for (int f = 0; f < arrayBuffer->FaceCount; f++) {
                int vertexCountPerFace = *faceSizePtr;
                vertexFirst = &arrayBuffer->VertexBuffer[faceInfoPtr->VerticesStartIndex];
                vertex = vertexFirst;

                Vector2 polygonVertex[4];
                int polygonVertexIndex = 0;
                while (vertexCountPerFace--) {
                    if (drawMode & 8) {
                        int vertexZ = vertex->Position.Z;
                        if (vertexZ < 0x100)
                            goto mrt_poly_solid_NEXT_FACE;

                        polygonVertex[polygonVertexIndex].X = ((vertex->Position.X << bitshiftX) / vertexZ << 16) - (cx << 16) + widthHalfSubpx;
                        polygonVertex[polygonVertexIndex].Y = ((vertex->Position.Y << bitshiftY) / vertexZ << 16) - (cy << 16) + heightHalfSubpx;
                    }
                    else {
                        polygonVertex[polygonVertexIndex].X = (vertex->Position.X << 8) - (cx << 16);
                        polygonVertex[polygonVertexIndex].Y = (vertex->Position.Y << 8) - (cy << 16);
                    }
                    polygonVertexIndex++;
                    vertex++;
                }
                DrawPolygon(polygonVertex, vertexFirst->Color, *faceSizePtr, opacity, blendFlag);

                mrt_poly_solid_NEXT_FACE:
                faceSizePtr++;
                faceInfoPtr++;
            }
            break;
        // Polygons, Flat Shading
        case 3:
            for (int f = 0; f < arrayBuffer->FaceCount; f++) {
                int vertexCountPerFace = *faceSizePtr;
                vertexFirst = &arrayBuffer->VertexBuffer[faceInfoPtr->VerticesStartIndex];
                vertex = vertexFirst;

                int averageNormalY = vertex[0].Normal.Y;
                switch (*faceSizePtr) {
                    case 2:
                        averageNormalY += vertex[1].Normal.Y;
                        break;
                    case 3:
                        averageNormalY += vertex[1].Normal.Y + vertex[2].Normal.Y;
                        break;
                    case 4:
                        averageNormalY += vertex[1].Normal.Y + vertex[2].Normal.Y + vertex[3].Normal.Y;
                        break;
                }
                averageNormalY /= *faceSizePtr;

                int color = vertex->Color;
                int col_r = (color >> 16) & 0xFF;
                int col_g = (color >> 8) & 0xFF;
                int col_b = (color) & 0xFF;
                int specularR = 0, specularG = 0, specularB = 0;

                int ambientNormalY = averageNormalY >> 10;
                int reweightedNormal = (averageNormalY >> 2) * (abs(averageNormalY) >> 2);

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

                // color
                color = col_r << 16 | col_g << 8 | col_b;

                Vector2 polygonVertex[4];
                int polygonVertexIndex = 0;
                while (vertexCountPerFace--) {
                    if (drawMode & 8) {
                        int vertexZ = vertex->Position.Z;
                        if (vertexZ < 0x100)
                            goto mrt_poly_flat_NEXT_FACE;

                        polygonVertex[polygonVertexIndex].X = ((vertex->Position.X << bitshiftX) / vertexZ << 16) - (cx << 16) + widthHalfSubpx;
                        polygonVertex[polygonVertexIndex].Y = ((vertex->Position.Y << bitshiftY) / vertexZ << 16) - (cy << 16) + heightHalfSubpx;
                    }
                    else {
                        polygonVertex[polygonVertexIndex].X = (vertex->Position.X << 8) - (cx << 16);
                        polygonVertex[polygonVertexIndex].Y = (vertex->Position.Y << 8) - (cy << 16);
                    }
                    polygonVertexIndex++;
                    vertex++;
                }
                DrawPolygon(polygonVertex, color, *faceSizePtr, opacity, blendFlag);

                mrt_poly_flat_NEXT_FACE:
                faceSizePtr++;
                faceInfoPtr++;
            }
            break;
        // Polygons, Smooth Shading
        case 5:
            for (int f = 0; f < arrayBuffer->FaceCount; f++) {
                int vertexCountPerFace = *faceSizePtr;
                vertexFirst = &arrayBuffer->VertexBuffer[faceInfoPtr->VerticesStartIndex];
                vertex = vertexFirst;

                Vector2 polygonVertex[4];
                int     polygonVertColor[4];
                int     polygonVertexIndex = 0;
                while (vertexCountPerFace--) {
                    if (drawMode & 8) {
                        int vertexZ = vertex->Position.Z;
                        if (vertexZ < 0x100)
                            goto mrt_poly_smooth_NEXT_FACE;

                        polygonVertex[polygonVertexIndex].X = ((vertex->Position.X << bitshiftX) / vertexZ << 16) - (cx << 16) + widthHalfSubpx;
                        polygonVertex[polygonVertexIndex].Y = ((vertex->Position.Y << bitshiftY) / vertexZ << 16) - (cy << 16) + heightHalfSubpx;
                    }
                    else {
                        polygonVertex[polygonVertexIndex].X = (vertex->Position.X << 8) - (cx << 16);
                        polygonVertex[polygonVertexIndex].Y = (vertex->Position.Y << 8) - (cy << 16);
                    }

                    int color = vertex->Color;
                    int col_r = (color >> 16) & 0xFF;
                    int col_g = (color >> 8) & 0xFF;
                    int col_b = (color) & 0xFF;
                    int specularR = 0, specularG = 0, specularB = 0;
                    int averageNormalY = vertex->Normal.Y;

                    int ambientNormalY = averageNormalY >> 10;
                    int reweightedNormal = (averageNormalY >> 2) * (abs(averageNormalY) >> 2);

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

                    // color
                    color = col_r << 16 | col_g << 8 | col_b;



                    polygonVertColor[polygonVertexIndex] = color;
                    polygonVertexIndex++;
                    vertex++;
                }
                DrawPolygonBlend(polygonVertex, polygonVertColor, *faceSizePtr, opacity, blendFlag);

                mrt_poly_smooth_NEXT_FACE:
                faceSizePtr++;
                faceInfoPtr++;
            }
            break;
    }
}
PUBLIC STATIC void     SoftwareRenderer::DrawModel(IModel* model, int frame, Matrix4x4i* viewMatrix, Matrix4x4i* normalMatrix) {
    VertexAttribute* arrayVertexItem;
    int arrayVertexCount, arrayFaceCount, modelVertexIndexCount;
    Uint8* faceSizeItem;
    Sint16* modelVertexIndexPtr;
    Vector3* positionPtr;
    Uint32* colorPtr;
    Vector2* uvPtr;

    int vertexTypeMask = VertexType_Position | VertexType_Normal | VertexType_Color;

    int color = ColRGB;

    #define APPLY_MAT4X4(vec3out, vec3in, M) \
        vec3out.X = M->Column[0][3] + ((vec3in.X * M->Column[0][0]) >> 8) + ((vec3in.Y * M->Column[0][1]) >> 8) + ((vec3in.Z * M->Column[0][2]) >> 8); \
        vec3out.Y = M->Column[1][3] + ((vec3in.X * M->Column[1][0]) >> 8) + ((vec3in.Y * M->Column[1][1]) >> 8) + ((vec3in.Z * M->Column[1][2]) >> 8); \
        vec3out.Z = M->Column[2][3] + ((vec3in.X * M->Column[2][0]) >> 8) + ((vec3in.Y * M->Column[2][1]) >> 8) + ((vec3in.Z * M->Column[2][2]) >> 8);

    while (frame >= model->FrameCount)
        frame -= model->FrameCount;
    frame *= model->VertexCount;

    Uint32 arrayBufferIndex = CurrentArrayBuffer;
    if (viewMatrix) {
        ArrayBuffer* arrayBuffer = &ArrayBuffers[arrayBufferIndex];
        if (!arrayBuffer->Initialized)
            return;

        arrayFaceCount = arrayBuffer->FaceCount;
        arrayVertexCount = arrayBuffer->VertexCount;

        faceSizeItem = &arrayBuffer->FaceSizeBuffer[arrayFaceCount];
        arrayVertexItem = &arrayBuffer->VertexBuffer[arrayVertexCount];

        modelVertexIndexCount = model->VertexIndexCount;
        modelVertexIndexPtr = model->VertexIndexBuffer;

        if (arrayVertexCount + modelVertexIndexCount <= arrayBuffer->VertexCapacity) {
            arrayBuffer->VertexCount += modelVertexIndexCount;
            arrayBuffer->FaceCount += modelVertexIndexCount / model->FaceVertexCount;

            switch (model->VertexFlag & vertexTypeMask) {
                case VertexType_Position:
                    // For every face,
                    while (*modelVertexIndexPtr != -1) {
                        int faceVertexCount = model->FaceVertexCount;
                        *faceSizeItem++ = faceVertexCount;

                        // For every vertex index,
                        while (faceVertexCount--) {
                            positionPtr = &model->PositionBuffer[(*modelVertexIndexPtr + frame)];
                            APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                            modelVertexIndexPtr++;
                            arrayVertexItem++;
                        }
                    }
                    break;
                case VertexType_Position | VertexType_Normal:
                    if (normalMatrix) {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->PositionBuffer[(*modelVertexIndexPtr + frame) << 1];
                                // Calculate position
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                // Calculate normals
                                APPLY_MAT4X4(arrayVertexItem->Normal, positionPtr[1], normalMatrix);
                                arrayVertexItem->Color = color;

                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    else {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->PositionBuffer[(*modelVertexIndexPtr + frame) << 1];
                                // Calculate position
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                arrayVertexItem->Normal = positionPtr[1];
                                arrayVertexItem->Color = color;
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    break;
                case VertexType_Position | VertexType_Normal | VertexType_Color:
                    if (normalMatrix) {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->PositionBuffer[(*modelVertexIndexPtr + frame) << 1];
                                colorPtr = &model->ColorBuffer[*modelVertexIndexPtr];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                APPLY_MAT4X4(arrayVertexItem->Normal, positionPtr[1], normalMatrix);
                                arrayVertexItem->Color = colorPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    else {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->PositionBuffer[(*modelVertexIndexPtr + frame) << 1];
                                colorPtr = &model->ColorBuffer[*modelVertexIndexPtr];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                arrayVertexItem->Normal = positionPtr[1];
                                arrayVertexItem->Color = colorPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    break;
                case VertexType_Position | VertexType_Normal | VertexType_UV:
                    if (normalMatrix) {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->PositionBuffer[(*modelVertexIndexPtr + frame) << 1];
                                uvPtr = &model->UVBuffer[(*modelVertexIndexPtr + frame)];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                APPLY_MAT4X4(arrayVertexItem->Normal, positionPtr[1], normalMatrix);
                                arrayVertexItem->Color = color;
                                arrayVertexItem->UV = uvPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    else {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->PositionBuffer[(*modelVertexIndexPtr + frame) << 1];
                                uvPtr = &model->UVBuffer[(*modelVertexIndexPtr + frame)];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                arrayVertexItem->Normal = positionPtr[1];
                                arrayVertexItem->Color = color;
                                arrayVertexItem->UV = uvPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    break;
                case VertexType_Position | VertexType_Normal | VertexType_UV | VertexType_Color:
                    if (normalMatrix) {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->PositionBuffer[(*modelVertexIndexPtr + frame) << 1];
                                uvPtr = &model->UVBuffer[(*modelVertexIndexPtr + frame)];
                                colorPtr = &model->ColorBuffer[*modelVertexIndexPtr];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                APPLY_MAT4X4(arrayVertexItem->Normal, positionPtr[1], normalMatrix);
                                arrayVertexItem->Color = colorPtr[0];
                                arrayVertexItem->UV = uvPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    else {
                        // For every face,
                        while (*modelVertexIndexPtr != -1) {
                            int faceVertexCount = model->FaceVertexCount;
                            *faceSizeItem++ = faceVertexCount;

                            // For every vertex index,
                            while (faceVertexCount--) {
                                positionPtr = &model->PositionBuffer[(*modelVertexIndexPtr + frame) << 1];
                                uvPtr = &model->UVBuffer[(*modelVertexIndexPtr + frame)];
                                colorPtr = &model->ColorBuffer[*modelVertexIndexPtr];
                                APPLY_MAT4X4(arrayVertexItem->Position, positionPtr[0], viewMatrix);
                                arrayVertexItem->Normal = positionPtr[1];
                                arrayVertexItem->Color = colorPtr[0];
                                arrayVertexItem->UV = uvPtr[0];
                                modelVertexIndexPtr++;
                                arrayVertexItem++;
                            }
                        }
                    }
                    break;
            }
        }
        else {
            BytecodeObjectManager::Threads[0].ThrowRuntimeError(false, "Model has too many vertices (%d) to fit in size (%d) of array buffer! Increase array buffer size by %d, or use a model with less vertices!", modelVertexIndexCount, arrayBuffer->VertexCapacity, (arrayVertexCount + modelVertexIndexCount) - arrayBuffer->VertexCapacity);
        }
    }

    #undef APPLY_MAT4X4
}

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

    int clip_x1 = 0,
        clip_y1 = 0,
        clip_x2 = 0,
        clip_y2 = 0;

    if (Graphics::CurrentClip.Enabled) {
        clip_x1 = Graphics::CurrentClip.X;
        clip_y1 = Graphics::CurrentClip.Y;
        clip_x2 = Graphics::CurrentClip.X + Graphics::CurrentClip.Width;
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

    #define DEFORM_X { \
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

    #define DRAW_PLACEPIXEL(pixelFunction) \
        if ((color = srcPxLine[src_x]) & 0xFF000000U) \
            pixelFunction(&color, &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);
    #define DRAW_PLACEPIXEL_PAL(pixelFunction) \
        if ((color = srcPxLine[src_x])) \
            pixelFunction(&index[color], &dstPxLine[dst_x], opacity, multTableAt, multSubTableAt);

    #define DRAW_NOFLIP(pixelFunction, placePixelMacro) \
    for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        srcPxLine = srcPx + src_strideY; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        if (SoftwareRenderer::UseSpriteDeform) \
            for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; dst_x++, src_x++) { \
                DEFORM_X; \
                placePixelMacro(pixelFunction) \
                dst_x -= *deformValues;\
            } \
        else \
            for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; dst_x++, src_x++) { \
                placePixelMacro(pixelFunction) \
            } \
        \
        dst_strideY += dstStride; src_strideY += srcStride; deformValues++; \
    }
    #define DRAW_FLIPX(pixelFunction, placePixelMacro) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        srcPxLine = srcPx + src_strideY; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        if (SoftwareRenderer::UseSpriteDeform) \
            for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; dst_x++, src_x--) { \
                DEFORM_X; \
                placePixelMacro(pixelFunction) \
                dst_x -= *deformValues;\
            } \
        else \
            for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; dst_x++, src_x--) { \
                placePixelMacro(pixelFunction) \
            } \
        dst_strideY += dstStride; src_strideY += srcStride; \
        deformValues++; \
    }
    #define DRAW_FLIPY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        srcPxLine = srcPx + src_strideY; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        if (SoftwareRenderer::UseSpriteDeform) \
            for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; dst_x++, src_x++) { \
                DEFORM_X; \
                placePixelMacro(pixelFunction) \
                dst_x -= *deformValues;\
            } \
        else \
            for (int dst_x = dst_x1, src_x = src_x1; dst_x < dst_x2; dst_x++, src_x++) { \
                placePixelMacro(pixelFunction) \
            } \
        dst_strideY += dstStride; src_strideY -= srcStride; \
        deformValues++; \
    }
    #define DRAW_FLIPXY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1; dst_y < dst_y2; dst_y++) { \
        srcPxLine = srcPx + src_strideY; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        if (SoftwareRenderer::UseSpriteDeform) \
            for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; dst_x++, src_x--) { \
                DEFORM_X; \
                placePixelMacro(pixelFunction) \
                dst_x -= *deformValues;\
            } \
        else \
            for (int dst_x = dst_x1, src_x = src_x2; dst_x < dst_x2; dst_x++, src_x--) { \
                placePixelMacro(pixelFunction) \
            } \
        dst_strideY += dstStride; src_strideY -= srcStride; \
        deformValues++; \
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
    Sint32* deformValues = &SoftwareRenderer::SpriteDeformBuffer[dst_y1];

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

    int cos = Cos0x200[rotation];
    int sin = Sin0x200[rotation];
    int rcos = Cos0x200[(0x200 - rotation + 0x200) & 0x1FF];
    int rsin = Sin0x200[(0x200 - rotation + 0x200) & 0x1FF];

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

    dst_x1 >>= 9;
    dst_y1 >>= 9;
    dst_x2 >>= 9;
    dst_y2 >>= 9;

    dst_x1 += x;
    dst_y1 += y;
    dst_x2 += x + 1;
    dst_y2 += y + 1;

    int clip_x1 = 0,
        clip_y1 = 0,
        clip_x2 = 0,
        clip_y2 = 0;

    if (Graphics::CurrentClip.Enabled) {
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

    #define DEFORM_X { \
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
        if (SoftwareRenderer::UseSpriteDeform) \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                DEFORM_X; \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x1 + (src_x - _x1) * sw / w); \
                    src_strideY = (src_y1 + (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
                dst_x -= *deformValues; \
            } \
        else \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x1 + (src_x - _x1) * sw / w); \
                    src_strideY = (src_y1 + (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
            } \
        dst_strideY += dstStride; deformValues++; \
    }
    #define DRAW_FLIPX(pixelFunction, placePixelMacro) for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
        i_y_rsin = -i_y * rsin; \
        i_y_rcos =  i_y * rcos; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        if (SoftwareRenderer::UseSpriteDeform) \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                DEFORM_X; \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x2 - (src_x - _x1) * sw / w); \
                    src_strideY = (src_y1 + (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
                dst_x -= *deformValues; \
            } \
        else \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x2 - (src_x - _x1) * sw / w); \
                    src_strideY = (src_y1 + (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
            } \
        dst_strideY += dstStride; deformValues++; \
    }
    #define DRAW_FLIPY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
        i_y_rsin = -i_y * rsin; \
        i_y_rcos =  i_y * rcos; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        if (SoftwareRenderer::UseSpriteDeform) \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                DEFORM_X; \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x1 + (src_x - _x1) * sw / w); \
                    src_strideY = (src_y2 - (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
                dst_x -= *deformValues; \
            } \
        else \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x1 + (src_x - _x1) * sw / w); \
                    src_strideY = (src_y2 - (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
            } \
        dst_strideY += dstStride; deformValues++; \
    }
    #define DRAW_FLIPXY(pixelFunction, placePixelMacro) for (int dst_y = dst_y1, i_y = dst_y1 - y; dst_y < dst_y2; dst_y++, i_y++) { \
        i_y_rsin = -i_y * rsin; \
        i_y_rcos =  i_y * rcos; \
        dstPxLine = dstPx + dst_strideY; \
        index = &SoftwareRenderer::PaletteColors[SoftwareRenderer::PaletteIndexLines[dst_y]][0]; \
        if (SoftwareRenderer::UseSpriteDeform) \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                DEFORM_X; \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x2 - (src_x - _x1) * sw / w); \
                    src_strideY = (src_y2 - (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
                dst_x -= *deformValues; \
            } \
        else \
            for (int dst_x = dst_x1, i_x = dst_x1 - x; dst_x < dst_x2; dst_x++, i_x++) { \
                src_x = (i_x * rcos + i_y_rsin) >> 9; \
                src_y = (i_x * rsin + i_y_rcos) >> 9; \
                if (src_x >= _x1 && src_y >= _y1 && \
                    src_x <  _x2 && src_y <  _y2) { \
                    src_x       = (src_x2 - (src_x - _x1) * sw / w); \
                    src_strideY = (src_y2 - (src_y - _y1) * sh / h) * srcStride; \
                    placePixelMacro(pixelFunction); \
                } \
            } \
        dst_strideY += dstStride; deformValues++; \
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
    Sint32* deformValues = &SoftwareRenderer::SpriteDeformBuffer[dst_y1];

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
        int rot = (int)(rotation * 0x100 / M_PI) & 0x1FF;
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

	/*

	bool AltPal = false;
	Uint32 PixelIndex, *Pal = sprite->Palette;
	AnimFrame animframe = sprite->Animations[animation].Frames[frame];

	srcw = Math::Min(srcw, animframe.Width- srcx);
	srch = Math::Min(srch, animframe.H - srcy);

	int DX = 0, DY = 0, PX, PY, size = srcw * srch;
	int CenterX = x, CenterY = y, RealCenterX = animframe.OffX, RealCenterY = animframe.OffY;
	int SrcX = animframe.X + srcx, SrcY = animframe.Y + srcy, Width = srcw - 1, Height = srch - 1;

	if (flipX)
		RealCenterX = -RealCenterX - Width - 1;
	if (flipY)
		RealCenterY = -RealCenterY - Height - 1;

	if (!AltPal && CenterY + RealCenterY >= WaterPaletteStartLine)
		AltPal = true, Pal = sprite->PaletteAlt;

	if (CenterX + Width + RealCenterX < Clip[0]) return;
	if (CenterY + Height + RealCenterY < Clip[1]) return;
	if (CenterX + RealCenterX >= Clip[2]) return;
	if (CenterY + RealCenterY >= Clip[3]) return;

	int FX, FY;
	for (int D = 0; D < size; D++) {
		PX = DX;
		PY = DY;
		if (flipX)
			PX = Width - PX;
		if (flipY)
			PY = Height - PY;

		FX = CenterX + DX + RealCenterX;
		FY = CenterY + DY + RealCenterY;
		if (FX <  Clip[0]) goto CONTINUE;
		if (FY <  Clip[1]) goto CONTINUE;
		if (FX >= Clip[2]) goto CONTINUE;
		if (FY >= Clip[3]) return;

		PixelIndex = GetPixelIndex(sprite, SrcX + PX, SrcY + PY);
		if (sprite->PaletteCount) {
			if (PixelIndex != sprite->TransparentColorIndex)
				SetPixelFunction(FX, FY, GetPixelOutputColor(Pal[PixelIndex]));
				// SetPixelFunction(FX, FY, Pal[PixelIndex]);
		}
		else if (PixelIndex & 0xFF000000U) {
			SetPixelFunction(FX, FY, GetPixelOutputColor(PixelIndex));
			// SetPixelFunction(FX, FY, PixelIndex);
		}

		CONTINUE:

		DX++;
		if (DX > Width) {
			DX = 0, DY++;

			if (!AltPal && CenterY + DY + RealCenterY >= WaterPaletteStartLine)
				AltPal = true, Pal = sprite->PaletteAlt;
		}
	}
	//*/

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
        int rot = (int)(rotation * 0x100 / M_PI) & 0x1FF;
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

        if (opacity == 0 && (blendFlag != BlendFlag_OPAQUE && blendFlag != BlendFlag_FILTER))
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
