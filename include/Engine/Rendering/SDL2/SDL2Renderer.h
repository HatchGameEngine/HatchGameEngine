#ifndef ENGINE_RENDERING_SDL2_SDL2RENDERER_H
#define ENGINE_RENDERING_SDL2_SDL2RENDERER_H

#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/ISprite.h>

namespace SDL2Renderer {
//private:
	SDL_BlendMode GetCustomBlendMode(int srcC, int dstC, int srcA, int dstA);

//public:
	extern float RenderScale;

	void Init();
	Uint32 GetWindowFlags();
	void SetVSync(bool enabled);
	void SetGraphicsFunctions();
	void Dispose();
	Texture* CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	int LockTexture(Texture* texture, void** pixels, int* pitch);
	int UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch);
	int UpdateTextureYUV(Texture* texture,
		SDL_Rect* src,
		void* pixelsY,
		int pitchY,
		void* pixelsU,
		int pitchU,
		void* pixelsV,
		int pitchV);
	void UnlockTexture(Texture* texture);
	void DisposeTexture(Texture* texture);
	void SetRenderTarget(Texture* texture);
	void CopyScreen(void* pixels, int width, int height);
	void UpdateWindowSize(int width, int height);
	void UpdateViewport();
	void UpdateClipRect();
	void UpdateOrtho(float left, float top, float right, float bottom);
	void UpdatePerspective(float fovy, float aspect, float nearv, float farv);
	void UpdateProjectionMatrix();
	void
	MakePerspectiveMatrix(Matrix4x4* out, float fov, float near, float far, float aspect);
	void GetMetalSize(int* width, int* height);
	void UseShader(void* shader);
	void SetUniformF(int location, int count, float* values);
	void SetUniformI(int location, int count, int* values);
	void SetUniformTexture(Texture* texture, int uniform_index, int slot);
	void Clear();
	void Present();
	void SetBlendColor(float r, float g, float b, float a);
	void SetBlendMode(int srcC, int dstC, int srcA, int dstA);
	void SetTintColor(float r, float g, float b, float a);
	void SetTintMode(int mode);
	void SetTintEnabled(bool enabled);
	void SetLineWidth(float n);
	void StrokeLine(float x1, float y1, float x2, float y2);
	void StrokeCircle(float x, float y, float rad, float thickness);
	void StrokeEllipse(float x, float y, float w, float h);
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
	void MakeFrameBufferID(ISprite* sprite);
};

#endif /* ENGINE_RENDERING_SDL2_SDL2RENDERER_H */
