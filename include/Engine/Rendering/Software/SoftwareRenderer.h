#ifndef ENGINE_RENDERING_SOFTWARE_SOFTWARERENDERER_H
#define ENGINE_RENDERING_SOFTWARE_SOFTWARERENDERER_H

#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Math/Clipper.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Rendering/Software/Contour.h>
#include <Engine/Rendering/Software/SoftwareEnums.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/ResourceTypes/ISprite.h>

class SoftwareRenderer {
private:
	static void SetColor(Uint32 color);
	static Uint32 GetBlendColor();
	static bool SetupPolygonRenderer(Matrix4x4* modelMatrix, Matrix4x4* normalMatrix);
	static void InitContour(Contour* contourBuffer, int dst_y1, int scanLineCount);
	static void RasterizeCircle(int ccx,
		int ccy,
		int dst_x1,
		int dst_y1,
		int dst_x2,
		int dst_y2,
		float rad,
		Contour* contourBuffer);
	static void StrokeThickCircle(float x, float y, float rad, float thickness);
	static void DrawShapeTextured(Texture* texturePtr,
		unsigned numPoints,
		float* px,
		float* py,
		int* pc,
		float* pu,
		float* pv);

public:
	static GraphicsFunctions BackendFunctions;
	static Uint32 CompareColor;
	static TileScanLine TileScanLineBuffer[MAX_FRAMEBUFFER_HEIGHT];
	static Sint32 SpriteDeformBuffer[MAX_FRAMEBUFFER_HEIGHT];
	static bool UseSpriteDeform;
	static Contour ContourBuffer[MAX_FRAMEBUFFER_HEIGHT];
	static int MultTable[0x10000];
	static int MultTableInv[0x10000];
	static int MultSubTable[0x10000];

	static void Init();
	static Uint32 GetWindowFlags();
	static void SetGraphicsFunctions();
	static void Dispose();
	static void RenderStart();
	static void RenderEnd();
	static Texture* CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	static int LockTexture(Texture* texture, void** pixels, int* pitch);
	static int UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch);
	static void UnlockTexture(Texture* texture);
	static void DisposeTexture(Texture* texture);
	static void SetRenderTarget(Texture* texture);
	static void ReadFramebuffer(void* pixels, int width, int height);
	static void UpdateWindowSize(int width, int height);
	static void UpdateViewport();
	static void UpdateClipRect();
	static void UpdateOrtho(float left, float top, float right, float bottom);
	static void UpdatePerspective(float fovy, float aspect, float nearv, float farv);
	static void UpdateProjectionMatrix();
	static void
	MakePerspectiveMatrix(Matrix4x4* out, float fov, float near, float far, float aspect);
	static void UseShader(void* shader);
	static void SetUniformF(int location, int count, float* values);
	static void SetUniformI(int location, int count, int* values);
	static void SetUniformTexture(Texture* texture, int uniform_index, int slot);
	static void SetFilter(int filter);
	static void Clear();
	static void Present();
	static void SetBlendColor(float r, float g, float b, float a);
	static void SetBlendMode(int srcC, int dstC, int srcA, int dstA);
	static void SetTintColor(float r, float g, float b, float a);
	static void SetTintMode(int mode);
	static void SetTintEnabled(bool enabled);
	static void Resize(int width, int height);
	static void SetClip(float x, float y, float width, float height);
	static void ClearClip();
	static void Save();
	static void Translate(float x, float y, float z);
	static void Rotate(float x, float y, float z);
	static void Scale(float x, float y, float z);
	static void Restore();
	static int ConvertBlendMode(int blendMode);
	static BlendState GetBlendState();
	static bool AlterBlendState(BlendState& state);
	static void PixelNoFiltSetOpaque(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelNoFiltSetTransparent(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelNoFiltSetAdditive(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelNoFiltSetSubtract(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelNoFiltSetMatchEqual(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelNoFiltSetMatchNotEqual(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelTintSetOpaque(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelTintSetTransparent(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelTintSetAdditive(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelTintSetSubtract(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelTintSetMatchEqual(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelTintSetMatchNotEqual(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void SetTintFunction(int blendFlags);
	static void SetStencilEnabled(bool enabled);
	static bool IsStencilEnabled();
	static void SetStencilTestFunc(int stencilTest);
	static void SetStencilPassFunc(int stencilOp);
	static void SetStencilFailFunc(int stencilOp);
	static void SetStencilValue(int value);
	static void SetStencilMask(int mask);
	static void ClearStencil();
	static void PixelStencil(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void SetDotMask(int mask);
	static void SetDotMaskH(int mask);
	static void SetDotMaskV(int mask);
	static void SetDotMaskOffsetH(int offset);
	static void SetDotMaskOffsetV(int offset);
	static void PixelDotMaskH(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelDotMaskV(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void PixelDotMaskHV(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	static void BindVertexBuffer(Uint32 vertexBufferIndex);
	static void UnbindVertexBuffer();
	static void BindScene3D(Uint32 sceneIndex);
	static void DrawScene3D(Uint32 sceneIndex, Uint32 drawMode);
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
	static PixelFunction GetPixelFunction(int blendFlag);
	static void SetLineWidth(float n);
	static void StrokeLine(float x1, float y1, float x2, float y2);
	static void StrokeCircle(float x, float y, float rad, float thickness);
	static void StrokeEllipse(float x, float y, float w, float h);
	static void StrokeRectangle(float x, float y, float w, float h);
	static void FillCircle(float x, float y, float rad);
	static void FillEllipse(float x, float y, float w, float h);
	static void FillRectangle(float x, float y, float w, float h);
	static void FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
	static void FillTriangleBlend(float x1,
		float y1,
		float x2,
		float y2,
		float x3,
		float y3,
		int c1,
		int c2,
		int c3);
	static void
	FillQuad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4);
	static void FillQuadBlend(float x1,
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
		int c4);
	static void DrawTriangleTextured(Texture* texturePtr,
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
		float v3);
	static void DrawQuadTextured(Texture* texturePtr,
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
		float v4);
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
	static void DrawTile(int tile, int x, int y, bool flipX, bool flipY);
	static void DrawSceneLayer_InitTileScanLines(SceneLayer* layer, View* currentView);
	static void DrawSceneLayer_HorizontalParallax(SceneLayer* layer, View* currentView);
	static void DrawSceneLayer_VerticalParallax(SceneLayer* layer, View* currentView);
	static void DrawSceneLayer_CustomTileScanLines(SceneLayer* layer, View* currentView);
	static void DrawSceneLayer(SceneLayer* layer,
		View* currentView,
		int layerIndex,
		bool useCustomFunction);
	static void MakeFrameBufferID(ISprite* sprite);
};

#endif /* ENGINE_RENDERING_SOFTWARE_SOFTWARERENDERER_H */
