#ifndef ENGINE_RENDERING_D3D_D3DRENDERER_H
#define ENGINE_RENDERING_D3D_D3DRENDERER_H

#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <stack>

namespace D3DRenderer {
//public:
	void Init();
	Uint32 GetWindowFlags();
	void SetVSync(bool enabled);
	void SetGraphicsFunctions();
	void Dispose();
	Texture* CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	int LockTexture(Texture* texture, void** pixels, int* pitch);
	int UpdateTexture(Texture* texture, SDL_Rect* r, void* pixels, int pitch);
	void UnlockTexture(Texture* texture);
	void DisposeTexture(Texture* texture);
	void SetRenderTarget(Texture* texture);
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
	Uint32 CreateTexturedShapeBuffer(float** data, int vertexCount);
	void DrawTexturedShapeBuffer(Texture* texture, Uint32 bufferID, int vertexCount);
};

#endif /* ENGINE_RENDERING_D3D_D3DRENDERER_H */
