#ifndef ENGINE_RENDERING_GL_GLRENDERER_H
#define ENGINE_RENDERING_GL_GLRENDERER_H

#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Rendering/GL/GLShaderBuilder.h>
#include <Engine/Rendering/GL/GLShaderContainer.h>
#include <Engine/Rendering/GL/Includes.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/ISprite.h>

class GLRenderer {
public:
	static SDL_GLContext Context;
	static GLShader* CurrentShader;
	static GLShaderContainer* ShaderShape;
	static GLShaderContainer* ShaderShape3D;
	static GLShaderContainer* ShaderFogLinear;
	static GLShaderContainer* ShaderFogExp;
	static GLShaderContainer* ShaderYUV;
	static GLint DefaultFramebuffer;
	static GLint DefaultRenderbuffer;
	static GLuint BufferCircleFill;
	static GLuint BufferCircleStroke;
	static GLuint BufferSquareFill;

	static void Init();
	static Uint32 GetWindowFlags();
	static void SetVSync(bool enabled);
	static void SetGraphicsFunctions();
	static void Dispose();
	static Texture* CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	static int LockTexture(Texture* texture, void** pixels, int* pitch);
	static int UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch);
	static int UpdateTextureYUV(Texture* texture,
		SDL_Rect* src,
		void* pixelsY,
		int pitchY,
		void* pixelsU,
		int pitchU,
		void* pixelsV,
		int pitchV);
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
	static void UpdateGlobalPalette();
	static void Clear();
	static void Present();
	static void SetBlendColor(float r, float g, float b, float a);
	static void SetBlendMode(int srcC, int dstC, int srcA, int dstA);
	static void SetTintColor(float r, float g, float b, float a);
	static void SetTintMode(int mode);
	static void SetTintEnabled(bool enabled);
	static void SetLineWidth(float n);
	static void StrokeLine(float x1, float y1, float x2, float y2);
	static void StrokeCircle(float x, float y, float rad, float thickness);
	static void StrokeEllipse(float x, float y, float w, float h);
	static void StrokeRectangle(float x, float y, float w, float h);
	static void FillCircle(float x, float y, float rad);
	static void FillEllipse(float x, float y, float w, float h);
	static void FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
	static void FillRectangle(float x, float y, float w, float h);
	static Uint32 CreateTexturedShapeBuffer(float* data, int vertexCount);
	static void DrawTexturedShapeBuffer(Texture* texture, Uint32 bufferID, int vertexCount);
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
	static void DrawModel(void* inModel,
		Uint16 animation,
		Uint32 frame,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	static void DrawModelSkinned(void* inModel,
		Uint16 armature,
		Matrix4x4* modelMatrix,
		Matrix4x4* normalMatrix);
	static void
	DrawVertexBuffer(Uint32 vertexBufferIndex, Matrix4x4* modelMatrix, Matrix4x4* normalMatrix);
	static void BindVertexBuffer(Uint32 vertexBufferIndex);
	static void UnbindVertexBuffer();
	static void ClearScene3D(Uint32 sceneIndex);
	static void DrawScene3D(Uint32 sceneIndex, Uint32 drawMode);
	static void* CreateVertexBuffer(Uint32 maxVertices);
	static void DeleteVertexBuffer(void* vtxBuf);
	static void MakeFrameBufferID(ISprite* sprite);
	static void DeleteFrameBufferID(ISprite* sprite);
	static void SetDepthTesting(bool enable);
};

#endif /* ENGINE_RENDERING_GL_GLRENDERER_H */
