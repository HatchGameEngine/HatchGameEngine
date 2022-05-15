#ifndef ENGINE_GRAPHICS_H
#define ENGINE_GRAPHICS_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class ISprite;
class IModel;

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Application.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene/View.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Rendering/Enums.h>

class Graphics {
public:
    static HashMap<Texture*>* TextureMap;
    static HashMap<Texture*>* SpriteSheetTextureMap;
    static bool   		      VsyncEnabled;
    static int 				  MultisamplingEnabled;
    static int 				  FontDPI;
    static bool   		      SupportsBatching;
    static bool   		      TextureBlend;
    static bool   		      TextureInterpolate;
    static Uint32             PreferredPixelFormat;
    static Uint32 		      MaxTextureWidth;
    static Uint32 		      MaxTextureHeight;
    static Texture*           TextureHead;
    static stack<Matrix4x4*>  ProjectionMatrixBackups;
    static stack<Matrix4x4*>  ModelViewMatrix;
    static Viewport           CurrentViewport;
    static Viewport           BackupViewport;
    static ClipArea           CurrentClip;
    static ClipArea           BackupClip;
    static float              BlendColors[4];
    static Texture*           CurrentRenderTarget;
    static void*              CurrentShader;
    static bool               SmoothFill;
    static bool               SmoothStroke;
    // Rendering functions
    static GraphicsFunctions  Internal;
    static GraphicsFunctions* GfxFunctions;
    static const char*        Renderer;
    static float              PixelOffset;
    static bool 			  NoInternalTextures;
    static int   			  BlendMode;
    static bool 			  UsePalettes;

    static void     Init();
    static void     ChooseBackend();
    static Uint32   GetWindowFlags();
    static void     Dispose();
    static Point    ProjectToScreen(float x, float y, float z);
    static Texture* CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
    static Texture* CreateTextureFromPixels(Uint32 width, Uint32 height, void* pixels, int pitch);
    static Texture* CreateTextureFromSurface(SDL_Surface* surface);
    static int      LockTexture(Texture* texture, void** pixels, int* pitch);
    static int      UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch);
    static int      UpdateYUVTexture(Texture* texture, SDL_Rect* src, Uint8* pixelsY, int pitchY, Uint8* pixelsU, int pitchU, Uint8* pixelsV, int pitchV);
    static void     UnlockTexture(Texture* texture);
    static void     DisposeTexture(Texture* texture);
    static void     UseShader(void* shader);
    static void     SetTextureInterpolation(bool interpolate);
    static void     Clear();
    static void     Present();
    static void     SoftwareStart();
    static void     SoftwareEnd();
    static void     FreeSceneMemory();
    static void     SetRenderTarget(Texture* texture);
    static void     UpdateOrtho(float width, float height);
    static void     UpdateOrthoFlipped(float width, float height);
    static void     UpdatePerspective(float fovy, float aspect, float nearv, float farv);
    static void     UpdateProjectionMatrix();
    static void     SetViewport(float x, float y, float w, float h);
    static void     ResetViewport();
    static void     Resize(int width, int height);
    static void     SetClip(int x, int y, int width, int height);
    static void     ClearClip();
    static void     Save();
    static void     Translate(float x, float y, float z);
    static void     Rotate(float x, float y, float z);
    static void     Scale(float x, float y, float z);
    static void     Restore();
    static void     SetBlendColor(float r, float g, float b, float a);
    static void     SetBlendMode(int blendMode);
    static void     SetBlendMode(int srcC, int dstC, int srcA, int dstA);
    static void     SetLineWidth(float n);
    static void     StrokeLine(float x1, float y1, float x2, float y2);
    static void     StrokeCircle(float x, float y, float rad);
    static void     StrokeEllipse(float x, float y, float w, float h);
    static void     StrokeTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
    static void     StrokeRectangle(float x, float y, float w, float h);
    static void     FillCircle(float x, float y, float rad);
    static void     FillEllipse(float x, float y, float w, float h);
    static void     FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
    static void     FillRectangle(float x, float y, float w, float h);
    static void     DrawTexture(Texture* texture, float sx, float sy, float sw, float sh, float x, float y, float w, float h);
    static void     DrawSprite(ISprite* sprite, int animation, int frame, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation);
    static void     DrawSpritePart(ISprite* sprite, int animation, int frame, int sx, int sy, int sw, int sh, int x, int y, bool flipX, bool flipY, float scaleW, float scaleH, float rotation);
    static void     DrawTile(int tile, int x, int y, bool flipX, bool flipY);
    static void     DrawSceneLayer_HorizontalParallax(SceneLayer* layer, View* currentView);
    static void     DrawSceneLayer_VerticalParallax(SceneLayer* layer, View* currentView);
    static void     DrawSceneLayer(SceneLayer* layer, View* currentView);
    static void     MakeFrameBufferID(ISprite* sprite, AnimFrame* frame);
    static bool     SpriteRangeCheck(ISprite* sprite, int animation, int frame);
};

#endif /* ENGINE_GRAPHICS_H */
