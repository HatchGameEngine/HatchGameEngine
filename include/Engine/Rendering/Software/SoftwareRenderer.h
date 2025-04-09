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

namespace SoftwareRenderer {
//private:
	void SetColor(Uint32 color);
	Uint32 GetBlendColor();
	bool SetupPolygonRenderer(Matrix4x4* modelMatrix, Matrix4x4* normalMatrix);
	void InitContour(Contour* contourBuffer, int dst_y1, int scanLineCount);
	void RasterizeCircle(int ccx,
		int ccy,
		int dst_x1,
		int dst_y1,
		int dst_x2,
		int dst_y2,
		float rad,
		Contour* contourBuffer);
	void StrokeThickCircle(float x, float y, float rad, float thickness);
	void DrawShapeTextured(Texture* texturePtr,
		unsigned numPoints,
		float* px,
		float* py,
		int* pc,
		float* pu,
		float* pv);

//public:
	extern GraphicsFunctions BackendFunctions;
	extern Uint32 CompareColor;
	extern TileScanLine TileScanLineBuffer[MAX_FRAMEBUFFER_HEIGHT];
	extern Sint32 SpriteDeformBuffer[MAX_FRAMEBUFFER_HEIGHT];
	extern bool UseSpriteDeform;
	extern Contour ContourBuffer[MAX_FRAMEBUFFER_HEIGHT];
	extern int MultTable[0x10000];
	extern int MultTableInv[0x10000];
	extern int MultSubTable[0x10000];

	void Init();
	Uint32 GetWindowFlags();
	void SetGraphicsFunctions();
	void Dispose();
	void RenderStart();
	void RenderEnd();
	Texture* CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	int LockTexture(Texture* texture, void** pixels, int* pitch);
	int UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch);
	void UnlockTexture(Texture* texture);
	void DisposeTexture(Texture* texture);
	void SetRenderTarget(Texture* texture);
	void ReadFramebuffer(void* pixels, int width, int height);
	void UpdateWindowSize(int width, int height);
	void UpdateViewport();
	void UpdateClipRect();
	void UpdateOrtho(float left, float top, float right, float bottom);
	void UpdatePerspective(float fovy, float aspect, float nearv, float farv);
	void UpdateProjectionMatrix();
	void
	MakePerspectiveMatrix(Matrix4x4* out, float fov, float near, float far, float aspect);
	void UseShader(void* shader);
	void SetUniformF(int location, int count, float* values);
	void SetUniformI(int location, int count, int* values);
	void SetUniformTexture(Texture* texture, int uniform_index, int slot);
	void SetFilter(int filter);
	void Clear();
	void Present();
	void SetBlendColor(float r, float g, float b, float a);
	void SetBlendMode(int srcC, int dstC, int srcA, int dstA);
	void SetTintColor(float r, float g, float b, float a);
	void SetTintMode(int mode);
	void SetTintEnabled(bool enabled);
	void Resize(int width, int height);
	void SetClip(float x, float y, float width, float height);
	void ClearClip();
	void Save();
	void Translate(float x, float y, float z);
	void Rotate(float x, float y, float z);
	void Scale(float x, float y, float z);
	void Restore();
	int ConvertBlendMode(int blendMode);
	BlendState GetBlendState();
	bool AlterBlendState(BlendState& state);
	void PixelNoFiltSetOpaque(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelNoFiltSetTransparent(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelNoFiltSetAdditive(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelNoFiltSetSubtract(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelNoFiltSetMatchEqual(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelNoFiltSetMatchNotEqual(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelTintSetOpaque(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelTintSetTransparent(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelTintSetAdditive(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelTintSetSubtract(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelTintSetMatchEqual(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelTintSetMatchNotEqual(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void SetTintFunction(int blendFlags);
	void SetStencilEnabled(bool enabled);
	bool IsStencilEnabled();
	void SetStencilTestFunc(int stencilTest);
	void SetStencilPassFunc(int stencilOp);
	void SetStencilFailFunc(int stencilOp);
	void SetStencilValue(int value);
	void SetStencilMask(int mask);
	void ClearStencil();
	void PixelStencil(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void SetDotMask(int mask);
	void SetDotMaskH(int mask);
	void SetDotMaskV(int mask);
	void SetDotMaskOffsetH(int offset);
	void SetDotMaskOffsetV(int offset);
	void PixelDotMaskH(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelDotMaskV(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void PixelDotMaskHV(Uint32* src,
		Uint32* dst,
		BlendState& state,
		int* multTableAt,
		int* multSubTableAt);
	void BindVertexBuffer(Uint32 vertexBufferIndex);
	void UnbindVertexBuffer();
	void BindScene3D(Uint32 sceneIndex);
	void DrawScene3D(Uint32 sceneIndex, Uint32 drawMode);
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
	PixelFunction GetPixelFunction(int blendFlag);
	void SetLineWidth(float n);
	void StrokeLine(float x1, float y1, float x2, float y2);
	void StrokeCircle(float x, float y, float rad, float thickness);
	void StrokeEllipse(float x, float y, float w, float h);
	void StrokeRectangle(float x, float y, float w, float h);
	void FillCircle(float x, float y, float rad);
	void FillEllipse(float x, float y, float w, float h);
	void FillRectangle(float x, float y, float w, float h);
	void FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
	void FillTriangleBlend(float x1,
		float y1,
		float x2,
		float y2,
		float x3,
		float y3,
		int c1,
		int c2,
		int c3);
	void
	FillQuad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4);
	void FillQuadBlend(float x1,
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
	void DrawTriangleTextured(Texture* texturePtr,
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
	void DrawQuadTextured(Texture* texturePtr,
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
	void DrawTile(int tile, int x, int y, bool flipX, bool flipY);
	void DrawSceneLayer_InitTileScanLines(SceneLayer* layer, View* currentView);
	void DrawSceneLayer_HorizontalParallax(SceneLayer* layer, View* currentView);
	void DrawSceneLayer_VerticalParallax(SceneLayer* layer, View* currentView);
	void DrawSceneLayer_CustomTileScanLines(SceneLayer* layer, View* currentView);
	void DrawSceneLayer(SceneLayer* layer,
		View* currentView,
		int layerIndex,
		bool useCustomFunction);
	void MakeFrameBufferID(ISprite* sprite);
};

#endif /* ENGINE_RENDERING_SOFTWARE_SOFTWARERENDERER_H */
