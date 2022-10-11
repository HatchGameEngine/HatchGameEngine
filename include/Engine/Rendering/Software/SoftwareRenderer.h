#ifndef ENGINE_RENDERING_SOFTWARE_SOFTWARERENDERER_H
#define ENGINE_RENDERING_SOFTWARE_SOFTWARERENDERER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


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

    static inline Uint32 GetPixelIndex(ISprite* sprite, int x, int y);
    static void    SetDrawAlpha(int a);
    static void    SetDrawFunc(int a);
    static void    SetFade(int fade);
    static void    SetFilter(int filter);
    static int     GetFilter();
    static bool    IsOnScreen(int x, int y, int w, int h);
    static void    DrawRectangleStroke(int x, int y, int width, int height, Uint32 color);
    static void    DrawRectangleSkewedH(int x, int y, int width, int height, int sk, Uint32 color);
    static void    DrawTextSprite(ISprite* sprite, int animation, char first, int x, int y, const char* string);
    static int     MeasureTextSprite(ISprite* sprite, int animation, char first, const char* string);
    static void    ConvertFromARGBtoNative(Uint32* argb, int count);
    static void     Init();
    static Uint32   GetWindowFlags();
    static void     SetGraphicsFunctions();
    static void     Dispose();
    static Texture* CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
    static int      LockTexture(Texture* texture, void** pixels, int* pitch);
    static int      UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch);
    static int      UpdateYUVTexture(Texture* texture, SDL_Rect* src, Uint8* pixelsY, int pitchY, Uint8* pixelsU, int pitchU, Uint8* pixelsV, int pitchV);
    static void     UnlockTexture(Texture* texture);
    static void     DisposeTexture(Texture* texture);
    static void     SetRenderTarget(Texture* texture);
    static void     UpdateWindowSize(int width, int height);
    static void     UpdateViewport();
    static void     UpdateClipRect();
    static void     UpdateOrtho(float left, float top, float right, float bottom);
    static void     UpdatePerspective(float fovy, float aspect, float nearv, float farv);
    static void     UpdateProjectionMatrix();
    static void     UseShader(void* shader);
    static void     SetUniformF(int location, int count, float* values);
    static void     SetUniformI(int location, int count, int* values);
    static void     SetUniformTexture(Texture* texture, int uniform_index, int slot);
    static void     Clear();
    static void     Present();
    static void     SetBlendColor(float r, float g, float b, float a);
    static void     SetBlendMode(int srcC, int dstC, int srcA, int dstA);
    static void     Resize(int width, int height);
    static void     SetClip(float x, float y, float width, float height);
    static void     ClearClip();
    static void     Save();
    static void     Translate(float x, float y, float z);
    static void     Rotate(float x, float y, float z);
    static void     Scale(float x, float y, float z);
    static void     Restore();
    static void     ArrayBuffer_Init(Uint32 arrayBufferIndex, Uint32 maxVertices);
    static void     ArrayBuffer_SetAmbientLighting(Uint32 arrayBufferIndex, Uint32 r, Uint32 g, Uint32 b);
    static void     ArrayBuffer_SetDiffuseLighting(Uint32 arrayBufferIndex, Uint32 r, Uint32 g, Uint32 b);
    static void     ArrayBuffer_SetSpecularLighting(Uint32 arrayBufferIndex, Uint32 r, Uint32 g, Uint32 b);
    static void     ArrayBuffer_DrawBegin(Uint32 arrayBufferIndex);
    static void     ArrayBuffer_DrawFinish(Uint32 arrayBufferIndex, Uint32 drawMode);
    static void     DrawModel(IModel* model, int frame, Matrix4x4i* viewMatrix, Matrix4x4i* normalMatrix);
    static void     SetLineWidth(float n);
    static void     StrokeLine(float x1, float y1, float x2, float y2);
    static void     StrokeCircle(float x, float y, float rad);
    static void     StrokeEllipse(float x, float y, float w, float h);
    static void     StrokeRectangle(float x, float y, float w, float h);
    static void     FillCircle(float x, float y, float rad);
    static void     FillEllipse(float x, float y, float w, float h);
    static void     FillRectangle(float x, float y, float w, float h);
    static void     FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
    static void     FillTriangleBlend(float x1, float y1, float x2, float y2, float x3, float y3, int c1, int c2, int c3);
    static void     FillQuadBlend(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, int c1, int c2, int c3, int c4);
    static void     DrawTexture(Texture* texture, float sx, float sy, float sw, float sh, float x, float y, float w, float h);
    static void     DrawSprite(ISprite* sprite, int animation, int frame, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation);
    static void     DrawSpritePart(ISprite* sprite, int animation, int frame, int sx, int sy, int sw, int sh, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation);
    static void     DrawTile(int tile, int x, int y, bool flipX, bool flipY);
    static void     DrawSceneLayer_InitTileScanLines(SceneLayer* layer, View* currentView);
    static void     DrawSceneLayer_HorizontalParallax(SceneLayer* layer, View* currentView);
    static void     DrawSceneLayer_VerticalParallax(SceneLayer* layer, View* currentView);
    static void     DrawSceneLayer_CustomTileScanLines(SceneLayer* layer, View* currentView);
    static void     DrawSceneLayer(SceneLayer* layer, View* currentView);
    static void     MakeFrameBufferID(ISprite* sprite, AnimFrame* frame);
};

#endif /* ENGINE_RENDERING_SOFTWARE_SOFTWARERENDERER_H */
