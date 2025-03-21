#ifndef ENGINE_GRAPHICS_H
#define ENGINE_GRAPHICS_H
class ISprite;
class IModel;

#include <Engine/Application.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/Enums.h>
#include <Engine/Rendering/GraphicsFunctions.h>
#include <Engine/Rendering/Scene3D.h>
#include <Engine/Rendering/TextureReference.h>
#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/Scene/SceneEnums.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene/View.h>
#include <Engine/Utilities/ColorUtils.h>

class Graphics {
public:
	static bool Initialized;
	static HashMap<Texture*>* TextureMap;
	static map<string, TextureReference*> SpriteSheetTextureMap;
	static bool VsyncEnabled;
	static int MultisamplingEnabled;
	static int FontDPI;
	static bool SupportsBatching;
	static bool TextureBlend;
	static bool TextureInterpolate;
	static Uint32 PreferredPixelFormat;
	static Uint32 MaxTextureWidth;
	static Uint32 MaxTextureHeight;
	static Texture* TextureHead;
	static vector<VertexBuffer*> VertexBuffers;
	static Scene3D Scene3Ds[MAX_3D_SCENES];
	static stack<GraphicsState> StateStack;
	static Matrix4x4 MatrixStack[MATRIX_STACK_SIZE];
	static size_t MatrixStackID;
	static Matrix4x4* ModelViewMatrix;
	static Viewport CurrentViewport;
	static Viewport BackupViewport;
	static ClipArea CurrentClip;
	static ClipArea BackupClip;
	static View* CurrentView;
	static float BlendColors[4];
	static float TintColors[4];
	static int BlendMode;
	static int TintMode;
	static int StencilTest;
	static int StencilOpPass;
	static int StencilOpFail;
	static void* FramebufferPixels;
	static size_t FramebufferSize;
	static Uint32 PaletteColors[MAX_PALETTE_COUNT][0x100];
	static Uint8 PaletteIndexLines[MAX_FRAMEBUFFER_HEIGHT];
	static bool PaletteUpdated;
	static Texture* PaletteTexture;
	static Texture* CurrentRenderTarget;
	static Sint32 CurrentScene3D;
	static Sint32 CurrentVertexBuffer;
	static void* CurrentShader;
	static bool SmoothFill;
	static bool SmoothStroke;
	static float PixelOffset;
	static bool NoInternalTextures;
	static bool UsePalettes;
	static bool UsePaletteIndexLines;
	static bool UseTinting;
	static bool UseDepthTesting;
	static bool UseSoftwareRenderer;
	static unsigned CurrentFrame;
	// Rendering functions
	static GraphicsFunctions Internal;
	static GraphicsFunctions* GfxFunctions;
	static const char* Renderer;

	static void Init();
	static void ChooseBackend();
	static Uint32 GetWindowFlags();
	static void SetVSync(bool enabled);
	static void Reset();
	static void Dispose();
	static Point ProjectToScreen(float x, float y, float z);
	static Texture* CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	static Texture*
	CreateTextureFromPixels(Uint32 width, Uint32 height, void* pixels, int pitch);
	static Texture* CreateTextureFromSurface(SDL_Surface* surface);
	static int LockTexture(Texture* texture, void** pixels, int* pitch);
	static int UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch);
	static int UpdateYUVTexture(Texture* texture,
		SDL_Rect* src,
		Uint8* pixelsY,
		int pitchY,
		Uint8* pixelsU,
		int pitchU,
		Uint8* pixelsV,
		int pitchV);
	static int SetTexturePalette(Texture* texture, void* palette, unsigned numPaletteColors);
	static int ConvertTextureToRGBA(Texture* texture);
	static int ConvertTextureToPalette(Texture* texture, unsigned paletteNumber);
	static void UnlockTexture(Texture* texture);
	static void DisposeTexture(Texture* texture);
	static TextureReference* GetSpriteSheet(string sheetPath);
	static TextureReference* AddSpriteSheet(string sheetPath, Texture* texture);
	static void DisposeSpriteSheet(string sheetPath);
	static void DeleteSpriteSheetMap();
	static Uint32 CreateVertexBuffer(Uint32 maxVertices, int unloadPolicy);
	static void DeleteVertexBuffer(Uint32 vertexBufferIndex);
	static void UseShader(void* shader);
	static void SetTextureInterpolation(bool interpolate);
	static void Clear();
	static void Present();
	static void SoftwareStart();
	static void SoftwareEnd();
	static void UpdateGlobalPalette();
	static void UnloadSceneData();
	static void SetRenderTarget(Texture* texture);
	static void CopyScreen(int source_x,
		int source_y,
		int source_w,
		int source_h,
		int dest_x,
		int dest_y,
		int dest_w,
		int dest_h,
		Texture* texture);
	static void UpdateOrtho(float width, float height);
	static void UpdateOrthoFlipped(float width, float height);
	static void UpdatePerspective(float fovy, float aspect, float nearv, float farv);
	static void UpdateProjectionMatrix();
	static void
	MakePerspectiveMatrix(Matrix4x4* out, float fov, float near, float far, float aspect);
	static void CalculateMVPMatrix(Matrix4x4* output,
		Matrix4x4* modelMatrix,
		Matrix4x4* viewMatrix,
		Matrix4x4* projMatrix);
	static void SetViewport(float x, float y, float w, float h);
	static void ResetViewport();
	static void Resize(int width, int height);
	static void SetClip(int x, int y, int width, int height);
	static void ClearClip();
	static void Save();
	static void Translate(float x, float y, float z);
	static void Rotate(float x, float y, float z);
	static void Scale(float x, float y, float z);
	static void Restore();
	static BlendState GetBlendState();
	static void PushState();
	static void PopState();
	static void SetBlendColor(float r, float g, float b, float a);
	static void SetBlendMode(int blendMode);
	static void SetBlendMode(int srcC, int dstC, int srcA, int dstA);
	static void SetTintColor(float r, float g, float b, float a);
	static void SetTintMode(int mode);
	static void SetTintEnabled(bool enabled);
	static void SetLineWidth(float n);
	static void StrokeLine(float x1, float y1, float x2, float y2);
	static void StrokeCircle(float x, float y, float rad, float thickness);
	static void StrokeEllipse(float x, float y, float w, float h);
	static void StrokeTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
	static void StrokeRectangle(float x, float y, float w, float h);
	static void FillCircle(float x, float y, float rad);
	static void FillEllipse(float x, float y, float w, float h);
	static void FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
	static void FillRectangle(float x, float y, float w, float h);
	static void DrawTexture(Texture* texture,
		float sx,
		float sy,
		float sw,
		float sh,
		float x,
		float y,
		float w,
		float h);
	static void DrawSprite(ISprite* sprite,
		int animation,
		int frame,
		int x,
		int y,
		bool flipX,
		bool flipY,
		float scaleW,
		float scaleH,
		float rotation,
		unsigned paletteID);
	static void DrawSpritePart(ISprite* sprite,
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
		unsigned paletteID);
	static void DrawSprite(ISprite* sprite,
		int animation,
		int frame,
		int x,
		int y,
		bool flipX,
		bool flipY,
		float scaleW,
		float scaleH,
		float rotation);
	static void DrawSpritePart(ISprite* sprite,
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
		float rotation);
	static void DrawTile(int tile, int x, int y, bool flipX, bool flipY);
	static void DrawSceneLayer_HorizontalParallax(SceneLayer* layer, View* currentView);
	static void DrawSceneLayer_VerticalParallax(SceneLayer* layer, View* currentView);
	static void DrawSceneLayer(SceneLayer* layer,
		View* currentView,
		int layerIndex,
		bool useCustomFunction);
	static void RunCustomSceneLayerFunction(ObjFunction* func, int layerIndex);
	static void DrawPolygon3D(void* data,
		int vertexCount,
		int vertexFlag,
		Texture* texture,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	static void DrawSceneLayer3D(void* layer,
		int sx,
		int sy,
		int sw,
		int sh,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	static void DrawModel(void* model,
		Uint16 animation,
		Uint32 frame,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	static void DrawModelSkinned(void* model,
		Uint16 armature,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	static void
	DrawVertexBuffer(Uint32 vertexBufferIndex, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix);
	static void BindVertexBuffer(Uint32 vertexBufferIndex);
	static void UnbindVertexBuffer();
	static void BindScene3D(Uint32 sceneIndex);
	static void ClearScene3D(Uint32 sceneIndex);
	static void DrawScene3D(Uint32 sceneIndex, Uint32 drawMode);
	static Uint32 CreateScene3D(int unloadPolicy);
	static void DeleteScene3D(Uint32 sceneIndex);
	static void InitScene3D(Uint32 sceneIndex, Uint32 numVertices);
	static void MakeSpritePolygon(VertexAttribute data[4],
		float x,
		float y,
		float z,
		int flipX,
		int flipY,
		float scaleX,
		float scaleY,
		Texture* texture,
		int frameX,
		int frameY,
		int frameW,
		int frameH);
	static void MakeSpritePolygonUVs(VertexAttribute data[4],
		int flipX,
		int flipY,
		Texture* texture,
		int frameX,
		int frameY,
		int frameW,
		int frameH);
	static void MakeFrameBufferID(ISprite* sprite);
	static void DeleteFrameBufferID(ISprite* sprite);
	static void SetDepthTesting(bool enabled);
	static bool SpriteRangeCheck(ISprite* sprite, int animation, int frame);
	static void ConvertFromARGBtoNative(Uint32* argb, int count);
	static void ConvertFromNativeToARGB(Uint32* argb, int count);
	static void SetStencilEnabled(bool enabled);
	static bool GetStencilEnabled();
	static void SetStencilTestFunc(int stencilTest);
	static void SetStencilPassFunc(int stencilOp);
	static void SetStencilFailFunc(int stencilOp);
	static void SetStencilValue(int value);
	static void SetStencilMask(int mask);
	static void ClearStencil();
};

#endif /* ENGINE_GRAPHICS_H */
