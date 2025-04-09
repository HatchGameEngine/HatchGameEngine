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

namespace Graphics {
//public:
	extern bool Initialized;
	extern HashMap<Texture*>* TextureMap;
	extern map<string, TextureReference*> SpriteSheetTextureMap;
	extern bool VsyncEnabled;
	extern int MultisamplingEnabled;
	extern int FontDPI;
	extern bool SupportsBatching;
	extern bool TextureBlend;
	extern bool TextureInterpolate;
	extern Uint32 PreferredPixelFormat;
	extern Uint32 MaxTextureWidth;
	extern Uint32 MaxTextureHeight;
	extern Texture* TextureHead;
	extern vector<VertexBuffer*> VertexBuffers;
	extern Scene3D Scene3Ds[MAX_3D_SCENES];
	extern stack<GraphicsState> StateStack;
	extern Matrix4x4 MatrixStack[MATRIX_STACK_SIZE];
	extern size_t MatrixStackID;
	extern Matrix4x4* ModelViewMatrix;
	extern Viewport CurrentViewport;
	extern Viewport BackupViewport;
	extern ClipArea CurrentClip;
	extern ClipArea BackupClip;
	extern View* CurrentView;
	extern float BlendColors[4];
	extern float TintColors[4];
	extern int BlendMode;
	extern int TintMode;
	extern int StencilTest;
	extern int StencilOpPass;
	extern int StencilOpFail;
	extern void* FramebufferPixels;
	extern size_t FramebufferSize;
	extern Uint32 PaletteColors[MAX_PALETTE_COUNT][0x100];
	extern Uint8 PaletteIndexLines[MAX_FRAMEBUFFER_HEIGHT];
	extern bool PaletteUpdated;
	extern Texture* PaletteTexture;
	extern Texture* CurrentRenderTarget;
	extern Sint32 CurrentScene3D;
	extern Sint32 CurrentVertexBuffer;
	extern void* CurrentShader;
	extern bool SmoothFill;
	extern bool SmoothStroke;
	extern float PixelOffset;
	extern bool NoInternalTextures;
	extern bool UsePalettes;
	extern bool UsePaletteIndexLines;
	extern bool UseTinting;
	extern bool UseDepthTesting;
	extern bool UseSoftwareRenderer;
	extern unsigned CurrentFrame;
	// Rendering functions
	extern GraphicsFunctions Internal;
	extern GraphicsFunctions* GfxFunctions;
	extern const char* Renderer;

	void Init();
	void ChooseBackend();
	Uint32 GetWindowFlags();
	void SetVSync(bool enabled);
	void Reset();
	void Dispose();
	Point ProjectToScreen(float x, float y, float z);
	Texture* CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	Texture*
	CreateTextureFromPixels(Uint32 width, Uint32 height, void* pixels, int pitch);
	Texture* CreateTextureFromSurface(SDL_Surface* surface);
	int LockTexture(Texture* texture, void** pixels, int* pitch);
	int UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch);
	int UpdateYUVTexture(Texture* texture,
		SDL_Rect* src,
		Uint8* pixelsY,
		int pitchY,
		Uint8* pixelsU,
		int pitchU,
		Uint8* pixelsV,
		int pitchV);
	int SetTexturePalette(Texture* texture, void* palette, unsigned numPaletteColors);
	int ConvertTextureToRGBA(Texture* texture);
	int ConvertTextureToPalette(Texture* texture, unsigned paletteNumber);
	void UnlockTexture(Texture* texture);
	void DisposeTexture(Texture* texture);
	TextureReference* GetSpriteSheet(string sheetPath);
	TextureReference* AddSpriteSheet(string sheetPath, Texture* texture);
	void DisposeSpriteSheet(string sheetPath);
	void DeleteSpriteSheetMap();
	Uint32 CreateVertexBuffer(Uint32 maxVertices, int unloadPolicy);
	void DeleteVertexBuffer(Uint32 vertexBufferIndex);
	void UseShader(void* shader);
	void SetTextureInterpolation(bool interpolate);
	void Clear();
	void Present();
	void SoftwareStart();
	void SoftwareEnd();
	void UpdateGlobalPalette();
	void UnloadSceneData();
	void SetRenderTarget(Texture* texture);
	void CopyScreen(int source_x,
		int source_y,
		int source_w,
		int source_h,
		int dest_x,
		int dest_y,
		int dest_w,
		int dest_h,
		Texture* texture);
	void UpdateOrtho(float width, float height);
	void UpdateOrthoFlipped(float width, float height);
	void UpdatePerspective(float fovy, float aspect, float nearv, float farv);
	void UpdateProjectionMatrix();
	void
	MakePerspectiveMatrix(Matrix4x4* out, float fov, float near, float far, float aspect);
	void CalculateMVPMatrix(Matrix4x4* output,
		Matrix4x4* modelMatrix,
		Matrix4x4* viewMatrix,
		Matrix4x4* projMatrix);
	void SetViewport(float x, float y, float w, float h);
	void ResetViewport();
	void Resize(int width, int height);
	void SetClip(int x, int y, int width, int height);
	void ClearClip();
	void Save();
	void Translate(float x, float y, float z);
	void Rotate(float x, float y, float z);
	void Scale(float x, float y, float z);
	void Restore();
	BlendState GetBlendState();
	void PushState();
	void PopState();
	void SetBlendColor(float r, float g, float b, float a);
	void SetBlendMode(int blendMode);
	void SetBlendMode(int srcC, int dstC, int srcA, int dstA);
	void SetTintColor(float r, float g, float b, float a);
	void SetTintMode(int mode);
	void SetTintEnabled(bool enabled);
	void SetLineWidth(float n);
	void StrokeLine(float x1, float y1, float x2, float y2);
	void StrokeCircle(float x, float y, float rad, float thickness);
	void StrokeEllipse(float x, float y, float w, float h);
	void StrokeTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
	void StrokeRectangle(float x, float y, float w, float h);
	void FillCircle(float x, float y, float rad);
	void FillEllipse(float x, float y, float w, float h);
	void FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
	void FillRectangle(float x, float y, float w, float h);
	void DrawTexture(Texture* texture,
		float sx,
		float sy,
		float sw,
		float sh,
		float x,
		float y,
		float w,
		float h);
	void DrawSprite(ISprite* sprite,
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
	void DrawSpritePart(ISprite* sprite,
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
	void DrawSprite(ISprite* sprite,
		int animation,
		int frame,
		int x,
		int y,
		bool flipX,
		bool flipY,
		float scaleW,
		float scaleH,
		float rotation);
	void DrawSpritePart(ISprite* sprite,
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
	void DrawTile(int tile, int x, int y, bool flipX, bool flipY);
	void DrawSceneLayer_HorizontalParallax(SceneLayer* layer, View* currentView);
	void DrawSceneLayer_VerticalParallax(SceneLayer* layer, View* currentView);
	void DrawSceneLayer(SceneLayer* layer,
		View* currentView,
		int layerIndex,
		bool useCustomFunction);
	void RunCustomSceneLayerFunction(ObjFunction* func, int layerIndex);
	void DrawPolygon3D(void* data,
		int vertexCount,
		int vertexFlag,
		Texture* texture,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	void DrawSceneLayer3D(void* layer,
		int sx,
		int sy,
		int sw,
		int sh,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	void DrawModel(void* model,
		Uint16 animation,
		Uint32 frame,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	void DrawModelSkinned(void* model,
		Uint16 armature,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	void
	DrawVertexBuffer(Uint32 vertexBufferIndex, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix);
	void BindVertexBuffer(Uint32 vertexBufferIndex);
	void UnbindVertexBuffer();
	void BindScene3D(Uint32 sceneIndex);
	void ClearScene3D(Uint32 sceneIndex);
	void DrawScene3D(Uint32 sceneIndex, Uint32 drawMode);
	Uint32 CreateScene3D(int unloadPolicy);
	void DeleteScene3D(Uint32 sceneIndex);
	void InitScene3D(Uint32 sceneIndex, Uint32 numVertices);
	void MakeSpritePolygon(VertexAttribute data[4],
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
	void MakeSpritePolygonUVs(VertexAttribute data[4],
		int flipX,
		int flipY,
		Texture* texture,
		int frameX,
		int frameY,
		int frameW,
		int frameH);
	void MakeFrameBufferID(ISprite* sprite);
	void DeleteFrameBufferID(ISprite* sprite);
	void SetDepthTesting(bool enabled);
	bool SpriteRangeCheck(ISprite* sprite, int animation, int frame);
	void ConvertFromARGBtoNative(Uint32* argb, int count);
	void ConvertFromNativeToARGB(Uint32* argb, int count);
	void SetStencilEnabled(bool enabled);
	bool GetStencilEnabled();
	void SetStencilTestFunc(int stencilTest);
	void SetStencilPassFunc(int stencilOp);
	void SetStencilFailFunc(int stencilOp);
	void SetStencilValue(int value);
	void SetStencilMask(int mask);
	void ClearStencil();
};

#endif /* ENGINE_GRAPHICS_H */
