#ifndef ENGINE_RENDERING_GRAPHICSFUNCTIONS
#define ENGINE_RENDERING_GRAPHICSFUNCTIONS

#include <Engine/Rendering/Shader.h>
#include <Engine/Rendering/Texture.h>

struct GraphicsFunctions {
	void (*Init)();
	Uint32 (*GetWindowFlags)();
	void (*SetVSync)(bool enable);
	void (*SetGraphicsFunctions)();
	void (*Dispose)();

	Texture* (*CreateTexture)(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	Texture* (*CreateTextureFromPixels)(Uint32 format,
		Uint32 width,
		Uint32 height,
		void* pixels,
		int pitch);
	bool (*ReinitializeTexture)(Texture* texture,
		Uint32 format,
		Uint32 access,
		Uint32 width,
		Uint32 height);
	int (*LockTexture)(Texture* texture, void** pixels, int* pitch);
	int (*UpdateTexture)(Texture* texture, SDL_Rect* src, void* pixels, int pitch);
	int (*UpdateYUVTexture)(Texture* texture,
		SDL_Rect* src,
		void* pixelsY,
		int pitchY,
		void* pixelsU,
		int pitchU,
		void* pixelsV,
		int pitchV);
	void (*CopyTexturePixels)(Texture* dest,
		int destX,
		int destY,
		Texture* src,
		int srcX,
		int srcY,
		int srcWidth,
		int srcHeight);
	void (*SetTextureMinFilter)(Texture* texture, int filterMode);
	void (*SetTextureMagFilter)(Texture* texture, int filterMode);
	int (*SetTexturePalette)(Texture* texture, void* palette, unsigned numPaletteColors);
	void (*UnlockTexture)(Texture* texture);
	void (*DisposeTexture)(Texture* texture);

	Shader* (*CreateShader)();
	void (*SetUserShader)(Shader* shader);

	void (*SetFilter)(int filter);

	void (*UpdateGlobalPalette)(Texture* texture);
	void (*UpdatePaletteIndexTable)(Texture* texture);

	void (*UpdateViewport)();
	void (*UpdateClipRect)();
	void (*UpdateOrtho)(float left, float top, float right, float bottom);
	void (*UpdatePerspective)(float fovy, float aspect, float near, float far);
	void (*UpdateProjectionMatrix)();
	void (*MakePerspectiveMatrix)(Matrix4x4* out,
		float fov,
		float near,
		float far,
		float aspect);

	void (*Clear)();
	void (*Present)();
	bool (*SetRenderTarget)(Texture* texture);
	void (*ReadFramebuffer)(void* pixels, int x, int y, int width, int height);
	void (*UpdateWindowSize)(int width, int height);

	void (*SetBlendColor)(float r, float g, float b, float a);
	void (*SetBlendMode)(int srcC, int dstC, int srcA, int dstA);
	void (*SetTintColor)(float r, float g, float b, float a);
	void (*SetTintMode)(int mode);
	void (*SetTintEnabled)(bool enabled);
	void (*SetLineWidth)(float n);

	void (*StrokeLine)(float x1, float y1, float x2, float y2);
	void (*StrokeCircle)(float x, float y, float rad, float thickness);
	void (*StrokeEllipse)(float x, float y, float w, float h);
	void (*StrokeRectangle)(float x, float y, float w, float h);
	void (*FillCircle)(float x, float y, float rad);
	void (*FillEllipse)(float x, float y, float w, float h);
	void (*FillRectangle)(float x, float y, float w, float h);
	void (*FillTriangle)(float x1, float y1, float x2, float y2, float x3, float y3);
	void (*FillTriangleBlend)(float* xc, float* yc, int* colors);
	void (*FillQuad)(float* xc, float* yc);
	void (*FillQuadBlend)(float* xc, float* yc, int* colors);
	void (*DrawTriangle)(Texture* texture,
		float* xc,
		float* yc,
		float* tu,
		float* tv,
		int* colors);
	void (*DrawQuad)(Texture* texture, float* xc, float* yc, float* tu, float* tv, int* colors);

	void (*DrawTexture)(Texture* texture,
		float sx,
		float sy,
		float sw,
		float sh,
		float x,
		float y,
		float w,
		float h,
		int paletteID);
	void (*DrawSprite)(ISprite* sprite,
		int animation,
		int frame,
		float x,
		float y,
		bool flipX,
		bool flipY,
		float scaleW,
		float scaleH,
		float rotation,
		int paletteID);
	void (*DrawSpritePart)(ISprite* sprite,
		int animation,
		int frame,
		int sx,
		int sy,
		int sw,
		int sh,
		float x,
		float y,
		bool flipX,
		bool flipY,
		float scaleW,
		float scaleH,
		float rotation,
		int paletteID);

	void (*DrawPolygon3D)(void* data,
		int vertexCount,
		int vertexFlag,
		Texture* texture,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	void (*DrawSceneLayer3D)(void* layer,
		int sx,
		int sy,
		int sw,
		int sh,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	void (*DrawModel)(void* model,
		Uint16 animation,
		Uint32 frame,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	void (*DrawModelSkinned)(void* model,
		Uint16 armature,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	void (*DrawVertexBuffer)(Uint32 vertexBufferIndex,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	void (*BindVertexBuffer)(Uint32 vertexBufferIndex);
	void (*UnbindVertexBuffer)();
	void (*BindScene3D)(Uint32 sceneIndex);
	void (*ClearScene3D)(Uint32 sceneIndex);
	void (*DrawScene3D)(Uint32 sceneIndex, Uint32 drawMode);

	void* (*CreateVertexBuffer)(Uint32 maxVertices);
	void (*DeleteVertexBuffer)(void* vtxBuf);
	void (*MakeFrameBufferID)(ISprite* sprite);
	void (*DeleteFrameBufferID)(ISprite* sprite);

	void (*SetStencilEnabled)(bool enabled);
	void (*SetStencilTestFunc)(int stencilTest);
	void (*SetStencilPassFunc)(int stencilOp);
	void (*SetStencilFailFunc)(int stencilOp);
	void (*SetStencilValue)(int value);
	void (*SetStencilMask)(int mask);
	void (*ClearStencil)();

	void (*SetDepthTesting)(bool enabled);
};

#endif /* ENGINE_RENDERING_GRAPHICSFUNCTIONS */
