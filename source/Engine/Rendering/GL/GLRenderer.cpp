#ifdef USING_OPENGL

// #define HAVE_GL_PERFSTATS

#define USE_USHORT_VTXBUFFER

#include <Engine/Rendering/GL/GLRenderer.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/ModelRenderer.h>
#include <Engine/Rendering/Scene3D.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/Utilities/ColorUtils.h>

#ifdef HAVE_GL_PERFSTATS
#include <Engine/Diagnostics/Clock.h>
#endif

SDL_GLContext GLRenderer::Context = NULL;

GLShader* GLRenderer::CurrentShader = NULL;

GLShaderContainer* GLRenderer::ShaderShape = NULL;
GLShaderContainer* GLRenderer::ShaderShape3D = NULL;
GLShaderContainer* GLRenderer::ShaderFogLinear = NULL;
GLShaderContainer* GLRenderer::ShaderFogExp = NULL;
GLShaderContainer* GLRenderer::ShaderYUV = NULL;

GLint GLRenderer::DefaultFramebuffer;
GLint GLRenderer::DefaultRenderbuffer;

GLuint GLRenderer::BufferCircleFill;
GLuint GLRenderer::BufferCircleStroke;
GLuint GLRenderer::BufferSquareFill;

bool UseDepthTesting = true;
float RetinaScale = 1.0;
Texture* GL_LastTexture = nullptr;

float FogTable[256];
float FogSmoothness = -1.0f;

PolygonRenderer polyRenderer;

// TODO:
// RetinaScale should belong to the texture (specifically
// TARGET_TEXTURES), and drawing functions should scale based on the
// current render target.

struct GL_Vec3 {
	float x;
	float y;
	float z;
};
struct GL_Vec2 {
	float x;
	float y;
};
struct GL_AnimFrameVert {
	float x;
	float y;
	float u;
	float v;
};
struct GL_TextureData {
	GLuint TextureID;
	GLuint TextureU;
	GLuint TextureV;
	bool YUV;
	bool Framebuffer;
	GLuint FBO;
	GLuint RBO;
	GLenum TextureTarget;
	GLenum TextureStorageFormat;
	GLenum PixelDataFormat;
	GLenum PixelDataType;
	int Slot;
	bool Accessed;
};
struct GL_VertexBufferEntry {
	float X, Y, Z;
	float TextureU, TextureV;
	float ColorR, ColorG, ColorB, ColorA;
	float NormalX, NormalY, NormalZ;
};
struct GL_VertexBufferFace {
	Uint32 NumVertices;
	Uint32 VertexIndex;
	bool UseMaterial;
	FaceMaterial MaterialInfo;
	Uint8 Opacity;
	Uint8 BlendMode;
	Uint32 DrawFlags;
	GLenum PrimitiveType;
	bool UseCulling;
	GLenum CullMode;
	GL_VertexBufferEntry* Data;
	GLshort VertexIndices[3];
};
struct GL_VertexBuffer {
	vector<GL_VertexBufferFace>* Faces;
	vector<GL_VertexBufferEntry*>* Entries;
	Uint32 Capacity;
	vector<vector<Uint32>*> VertexIndexList;
	void* VertexIndexBuffer;
	size_t VertexIndexBufferCapacity;
	bool UseVertexIndices;
	bool Changed;
};
struct GL_State {
	GL_VertexBufferEntry* VertexAtrribs;
	bool CullFace;
	unsigned CullMode;
	unsigned WindingOrder;
	GLShader* Shader;
	Texture* TexturePtr;
	bool UseMaterial;
	bool UseTexture;
	bool UsePalette;
	bool DepthMask;
	unsigned BlendMode;
	Uint32 DrawFlags;
	GLenum PrimitiveType;
	float DiffuseColor[4];
	float SpecularColor[4];
	float AmbientColor[4];
	float FogColor[4];
	float FogParams[4];
	unsigned FogMode;
};
struct GL_PerfStats {
	unsigned StateChanges;
	unsigned DrawCalls;
	bool UpdatedVertexBuffer;
	double Time;
};
struct GL_Scene3DBatch {
	bool ShouldDraw;
	vector<Uint32> VertexIndices;
};

GLenum GL_VertexIndexBufferFormat;
size_t GL_VertexIndexBufferMaxElements;
size_t GL_VertexIndexBufferStride;

GLenum GL_ActiveTexture;
GLenum GL_ActiveCullMode;
bool GL_ClippingEnabled;

#ifdef HAVE_GL_PERFSTATS
#define PERF_START(p) (p).Time = Clock::GetTicks()
#define PERF_STATE_CHANGE(p) (p).StateChanges++
#define PERF_DRAW_CALL(p) (p).DrawCalls++
#define PERF_END(p) (p).Time = Clock::GetTicks() - (p).Time
#else
#define PERF_START(p)
#define PERF_STATE_CHANGE(p)
#define PERF_DRAW_CALL(p)
#define PERF_END(p)
#endif

#define GL_SUPPORTS_MULTISAMPLING
#define GL_SUPPORTS_SMOOTHING
#define GL_SUPPORTS_RENDERBUFFER
#define GL_MONOCHROME_PIXELFORMAT GL_RED

#if GL_ES_VERSION_2_0 || GL_ES_VERSION_3_0
#define GL_ES
#undef GL_SUPPORTS_MULTISAMPLING
#undef GL_SUPPORTS_SMOOTHING
#undef GL_MONOCHROME_PIXELFORMAT
#define GL_MONOCHROME_PIXELFORMAT GL_LUMINANCE
#endif

void GL_MakeShaders() {
	GLRenderer::ShaderShape = GLShaderContainer::Make();
	GLRenderer::ShaderYUV = GLShaderContainer::MakeYUV();

	GLRenderer::ShaderShape3D = GLShaderContainer::Make(true, true);

	GLRenderer::ShaderFogLinear = GLShaderContainer::MakeFog(FogEquation_Linear);
	GLRenderer::ShaderFogExp = GLShaderContainer::MakeFog(FogEquation_Exp);
}
void GL_MakeShapeBuffers() {
	GL_Vec2 verticesSquareFill[4];
	GL_Vec3 verticesCircleFill[362];
	GL_Vec3 verticesCircleStroke[361];

	// Fill Square
	verticesSquareFill[0] = GL_Vec2{0.0f, 0.0f};
	verticesSquareFill[1] = GL_Vec2{1.0f, 0.0f};
	verticesSquareFill[2] = GL_Vec2{0.0f, 1.0f};
	verticesSquareFill[3] = GL_Vec2{1.0f, 1.0f};
	glGenBuffers(1, &GLRenderer::BufferSquareFill);
	glBindBuffer(GL_ARRAY_BUFFER, GLRenderer::BufferSquareFill);
	glBufferData(
		GL_ARRAY_BUFFER, sizeof(verticesSquareFill), verticesSquareFill, GL_STATIC_DRAW);
	CHECK_GL();

	// Filled Circle
	verticesCircleFill[0] = GL_Vec3{0.0f, 0.0f, 0.0f};
	for (int i = 0; i < 361; i++) {
		verticesCircleFill[i + 1] =
			GL_Vec3{(float)cos(i * M_PI / 180.0f), (float)sin(i * M_PI / 180.0f), 0.0f};
	}
	glGenBuffers(1, &GLRenderer::BufferCircleFill);
	glBindBuffer(GL_ARRAY_BUFFER, GLRenderer::BufferCircleFill);
	glBufferData(
		GL_ARRAY_BUFFER, sizeof(verticesCircleFill), verticesCircleFill, GL_STATIC_DRAW);
	CHECK_GL();

	// Stroke Circle
	for (int i = 0; i < 361; i++) {
		verticesCircleStroke[i] =
			GL_Vec3{(float)cos(i * M_PI / 180.0f), (float)sin(i * M_PI / 180.0f), 0.0f};
	}
	glGenBuffers(1, &GLRenderer::BufferCircleStroke);
	glBindBuffer(GL_ARRAY_BUFFER, GLRenderer::BufferCircleStroke);
	glBufferData(GL_ARRAY_BUFFER,
		sizeof(verticesCircleStroke),
		verticesCircleStroke,
		GL_STATIC_DRAW);
	CHECK_GL();

	// Reset buffer
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
void GL_SetTextureWrap(GL_TextureData* textureData,
	GLenum wrapS,
	GLenum wrapT = 0,
	GLenum binding = GL_TEXTURE_BINDING_2D) {
	if (!wrapT) {
		wrapT = wrapS;
	}

	GLint bound;
	glGetIntegerv(binding, &bound);
	glBindTexture(textureData->TextureTarget, textureData->TextureID);

	glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_T, wrapT);

	if (textureData->YUV) {
		glBindTexture(textureData->TextureTarget, textureData->TextureU);
		glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_S, wrapS);
		glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_T, wrapT);

		glBindTexture(textureData->TextureTarget, textureData->TextureV);
		glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_S, wrapS);
		glTexParameteri(textureData->TextureTarget, GL_TEXTURE_WRAP_T, wrapT);
	}

	glBindTexture(textureData->TextureTarget, bound);
}
void GL_BindTexture(Texture* texture, GLenum wrapS = 0, GLenum wrapT = 0) {
	// Do texture (re-)binding if necessary
	if (GL_LastTexture != texture) {
		GL_TextureData* textureData = nullptr;
		if (texture) {
			textureData = (GL_TextureData*)texture->DriverData;
		}

		if (textureData) {
			if (GL_ActiveTexture != GL_TEXTURE0) {
				glActiveTexture(GL_ActiveTexture = GL_TEXTURE0);
			}
			glBindTexture(GL_TEXTURE_2D, textureData->TextureID);

			if (!textureData->Accessed) {
				if (wrapS && wrapS != GL_REPEAT) { // GL_REPEAT
					// is the
					// default
					GL_SetTextureWrap(textureData, wrapS, wrapT);
				}

				textureData->Accessed = true;
			}
		}
		else {
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}
	GL_LastTexture = texture;
}
void GL_PreparePaletteShader() {
	glActiveTexture(GL_ActiveTexture = GL_TEXTURE1);
	glUniform1i(GLRenderer::CurrentShader->LocPalette, 1);

	if (Graphics::PaletteTexture && Graphics::PaletteTexture->DriverData) {
		GL_TextureData* paletteTexture =
			(GL_TextureData*)Graphics::PaletteTexture->DriverData;
		glBindTexture(GL_TEXTURE_2D, paletteTexture->TextureID);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	glActiveTexture(GL_ActiveTexture = GL_TEXTURE0);
}
void GL_SetTexture(Texture* texture) {
	// Use appropriate shader if changed
	if (texture) {
		GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;
		if (textureData && textureData->YUV) {
			GLRenderer::UseShader(GLRenderer::ShaderYUV->Textured);

			glActiveTexture(GL_ActiveTexture = GL_TEXTURE0);
			glUniform1i(GLRenderer::CurrentShader->LocTexture, 0);
			glBindTexture(GL_TEXTURE_2D, textureData->TextureID);

			glActiveTexture(GL_ActiveTexture = GL_TEXTURE1);
			glUniform1i(GLRenderer::CurrentShader->LocTextureU, 1);
			glBindTexture(GL_TEXTURE_2D, textureData->TextureU);

			glActiveTexture(GL_ActiveTexture = GL_TEXTURE2);
			glUniform1i(GLRenderer::CurrentShader->LocTextureV, 2);
			glBindTexture(GL_TEXTURE_2D, textureData->TextureV);
		}
		else {
			if (texture->Paletted && Graphics::UsePalettes) {
				GLRenderer::UseShader(GLRenderer::ShaderShape->Get(true, true));
				GL_PreparePaletteShader();
			}
			else {
				GLRenderer::UseShader(GLRenderer::ShaderShape->Get(true));
			}
		}

		GLint active;

		glGetVertexAttribiv(GLRenderer::CurrentShader->LocTexCoord,
			GL_VERTEX_ATTRIB_ARRAY_ENABLED,
			&active);
		if (!active) {
			glEnableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord);
		}
	}
	else {
		if (GLRenderer::CurrentShader == GLRenderer::ShaderShape->Textured ||
			GLRenderer::CurrentShader == GLRenderer::ShaderShape3D->Textured ||
			GLRenderer::CurrentShader == GLRenderer::ShaderYUV->Textured) {
			glDisableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord);
		}

		GLRenderer::UseShader(GLRenderer::ShaderShape->Get());
	}

	GL_BindTexture(texture);
}
void GL_SetProjectionMatrix(Matrix4x4* projMat) {
	if (!Matrix4x4::Equals(GLRenderer::CurrentShader->CachedProjectionMatrix, projMat)) {
		if (!GLRenderer::CurrentShader->CachedProjectionMatrix) {
			GLRenderer::CurrentShader->CachedProjectionMatrix = Matrix4x4::Create();
		}

		Matrix4x4::Copy(GLRenderer::CurrentShader->CachedProjectionMatrix, projMat);

		glUniformMatrix4fv(GLRenderer::CurrentShader->LocProjectionMatrix,
			1,
			false,
			GLRenderer::CurrentShader->CachedProjectionMatrix->Values);
	}
}
void GL_SetModelViewMatrix(Matrix4x4* modelViewMatrix) {
	if (!Matrix4x4::Equals(GLRenderer::CurrentShader->CachedModelViewMatrix, modelViewMatrix)) {
		if (!GLRenderer::CurrentShader->CachedModelViewMatrix) {
			GLRenderer::CurrentShader->CachedModelViewMatrix = Matrix4x4::Create();
		}

		Matrix4x4::Copy(GLRenderer::CurrentShader->CachedModelViewMatrix, modelViewMatrix);

		glUniformMatrix4fv(GLRenderer::CurrentShader->LocModelViewMatrix,
			1,
			false,
			GLRenderer::CurrentShader->CachedModelViewMatrix->Values);
	}
}
void GL_Predraw(Texture* texture) {
	GL_SetTexture(texture);

	// Update color if needed
	if (memcmp(&GLRenderer::CurrentShader->CachedBlendColors[0],
		    &Graphics::BlendColors[0],
		    sizeof(float) * 4) != 0) {
		memcpy(&GLRenderer::CurrentShader->CachedBlendColors[0],
			&Graphics::BlendColors[0],
			sizeof(float) * 4);

		glUniform4f(GLRenderer::CurrentShader->LocColor,
			Graphics::BlendColors[0],
			Graphics::BlendColors[1],
			Graphics::BlendColors[2],
			Graphics::BlendColors[3]);
	}

	// Update matrices
	GL_SetProjectionMatrix(Scene::Views[Scene::ViewCurrent].ProjectionMatrix);
	GL_SetModelViewMatrix(Graphics::ModelViewMatrix);
}
void GL_DrawTextureBuffered(Texture* texture, GLuint buffer, int offset, int flip) {
	GL_Predraw(texture);

	if (!Graphics::TextureBlend) {
		GLRenderer::CurrentShader->CachedBlendColors[0] =
			GLRenderer::CurrentShader->CachedBlendColors[1] =
				GLRenderer::CurrentShader->CachedBlendColors[2] =
					GLRenderer::CurrentShader->CachedBlendColors[3] = 1.0;
		glUniform4f(GLRenderer::CurrentShader->LocColor, 1.0, 1.0, 1.0, 1.0);
	}

	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glVertexAttribPointer(GLRenderer::CurrentShader->LocPosition,
		2,
		GL_FLOAT,
		GL_FALSE,
		sizeof(GL_AnimFrameVert),
		(void*)(uintptr_t)offset);
	glVertexAttribPointer(GLRenderer::CurrentShader->LocTexCoord,
		2,
		GL_FLOAT,
		GL_FALSE,
		sizeof(GL_AnimFrameVert),
		(void*)((uintptr_t)offset + 8));

	glDrawArrays(GL_TRIANGLE_STRIP, flip << 2, 4);
	CHECK_GL();
}
void GL_DrawTexture(Texture* texture,
	float sx,
	float sy,
	float sw,
	float sh,
	float x,
	float y,
	float w,
	float h) {
	GL_Predraw(texture);

	if (!Graphics::TextureBlend) {
		GLRenderer::CurrentShader->CachedBlendColors[0] =
			GLRenderer::CurrentShader->CachedBlendColors[1] =
				GLRenderer::CurrentShader->CachedBlendColors[2] =
					GLRenderer::CurrentShader->CachedBlendColors[3] = 1.0;
		glUniform4f(GLRenderer::CurrentShader->LocColor, 1.0, 1.0, 1.0, 1.0);
	}

	GL_Vec2 v[4];
	v[0] = GL_Vec2{x, y};
	v[1] = GL_Vec2{x + w, y};
	v[2] = GL_Vec2{x, y + h};
	v[3] = GL_Vec2{x + w, y + h};
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glVertexAttribPointer(GLRenderer::CurrentShader->LocPosition, 2, GL_FLOAT, GL_FALSE, 0, v);

	GL_Vec2 v2[4];
	if (sx >= 0.0) {
		v2[0] = GL_Vec2{(sx) / texture->Width, (sy) / texture->Height};
		v2[1] = GL_Vec2{(sx + sw) / texture->Width, (sy) / texture->Height};
		v2[2] = GL_Vec2{(sx) / texture->Width, (sy + sh) / texture->Height};
		v2[3] = GL_Vec2{(sx + sw) / texture->Width, (sy + sh) / texture->Height};
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glVertexAttribPointer(
			GLRenderer::CurrentShader->LocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, v2);
	}
	else {
		glBindBuffer(GL_ARRAY_BUFFER, GLRenderer::BufferSquareFill);
		glVertexAttribPointer(
			GLRenderer::CurrentShader->LocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
	}

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	CHECK_GL();
}
GLenum GL_GetBlendFactorFromHatchEnum(int factor) {
	switch (factor) {
	case BlendFactor_ZERO:
		return GL_ZERO;
	case BlendFactor_ONE:
		return GL_ONE;
	case BlendFactor_SRC_COLOR:
		return GL_SRC_COLOR;
	case BlendFactor_INV_SRC_COLOR:
		return GL_ONE_MINUS_SRC_COLOR;
	case BlendFactor_SRC_ALPHA:
		return GL_SRC_ALPHA;
	case BlendFactor_INV_SRC_ALPHA:
		return GL_ONE_MINUS_SRC_ALPHA;
	case BlendFactor_DST_COLOR:
		return GL_DST_COLOR;
	case BlendFactor_INV_DST_COLOR:
		return GL_ONE_MINUS_DST_COLOR;
	case BlendFactor_DST_ALPHA:
		return GL_DST_ALPHA;
	case BlendFactor_INV_DST_ALPHA:
		return GL_ONE_MINUS_DST_ALPHA;
	}
	return 0;
}
void GL_SetBlendFuncByMode(int mode) {
	switch (mode) {
	case BlendMode_NORMAL:
		GLRenderer::SetBlendMode(BlendFactor_SRC_ALPHA,
			BlendFactor_INV_SRC_ALPHA,
			BlendFactor_ONE,
			BlendFactor_ONE);
		break;
	case BlendMode_ADD:
		GLRenderer::SetBlendMode(
			BlendFactor_SRC_ALPHA, BlendFactor_ONE, BlendFactor_ONE, BlendFactor_ONE);
		break;
	case BlendMode_MAX:
		GLRenderer::SetBlendMode(BlendFactor_SRC_ALPHA,
			BlendFactor_INV_SRC_COLOR,
			BlendFactor_ONE,
			BlendFactor_ONE);
		break;
	case BlendMode_SUBTRACT:
		GLRenderer::SetBlendMode(BlendFactor_ZERO,
			BlendFactor_INV_SRC_COLOR,
			BlendFactor_ONE,
			BlendFactor_ONE);
		break;
	default:
		GLRenderer::SetBlendMode(BlendFactor_SRC_ALPHA,
			BlendFactor_INV_SRC_ALPHA,
			BlendFactor_ONE,
			BlendFactor_ONE);
	}
}
void GL_ReallocVertexBuffer(GL_VertexBuffer* vtxbuf, Uint32 maxVertices) {
	vtxbuf->Capacity = maxVertices;

	if (vtxbuf->Faces == nullptr) {
		vtxbuf->Faces = new vector<GL_VertexBufferFace>();
	}

	if (vtxbuf->Entries == nullptr) {
		vtxbuf->Entries = new vector<GL_VertexBufferEntry*>();
		vtxbuf->Entries->push_back(nullptr);
	}

	for (size_t e = 0; e < vtxbuf->Entries->size(); e++) {
		GL_VertexBufferEntry* entries = (*vtxbuf->Entries)[e];
		if (vtxbuf->Entries == nullptr) {
			(*vtxbuf->Entries)[e] = (GL_VertexBufferEntry*)Memory::TrackedCalloc(
				"GL_VertexBuffer::Entries",
				maxVertices,
				sizeof(GL_VertexBufferEntry));
		}
		else {
			(*vtxbuf->Entries)[e] = (GL_VertexBufferEntry*)Memory::Realloc(
				entries, maxVertices * sizeof(GL_VertexBufferEntry));
		}
	}
}
float GL_ProjectPointDepth(VertexAttribute* vertex, Matrix4x4* projMatrix) {
	float* M = projMatrix->Values;
	float vecX = FP16_FROM(vertex->Position.X);
	float vecY = FP16_FROM(vertex->Position.Y);
	float vecZ = FP16_FROM(vertex->Position.Z);
	// float z = M[11] + (vecX * M[ 8]) + (vecY * M[ 9]) + (vecZ *
	// M[10]);
	float w = M[15] + (vecX * M[12]) + (vecY * M[13]) + (vecZ * M[14]);

	// FIXME: ouch
	return -w;
}
GLenum GL_GetPrimitiveType(Uint32 drawMode) {
	switch (drawMode & DrawMode_PrimitiveMask) {
	case DrawMode_POLYGONS:
		return GL_TRIANGLE_FAN;
	case DrawMode_LINES:
		return GL_LINE_LOOP;
	default:
		return GL_POINTS;
	}
}
void GL_PrepareVertexBufferUpdate(Scene3D* scene, VertexBuffer* vertexBuffer, Uint32 drawMode) {
	GL_VertexBuffer* driverData = (GL_VertexBuffer*)vertexBuffer->DriverData;
	if (driverData->Capacity != vertexBuffer->Capacity) {
		GL_ReallocVertexBuffer(driverData, vertexBuffer->Capacity);
	}

	bool sortFaces = false;

	// Get the vertices' start index
	Uint32 verticesStartIndex = 0;
	for (Uint32 f = 0; f < vertexBuffer->FaceCount; f++) {
		FaceInfo* face = &vertexBuffer->FaceInfoBuffer[f];

		if (Graphics::TextureBlend && (drawMode & DrawMode_DEPTH_TEST)) {
			if (face->Blend.Opacity != 0xFF &&
				!(face->Blend.Opacity == 0 &&
					face->Blend.Mode == BlendMode_NORMAL)) {
				sortFaces = true;
			}
		}

		face->VerticesStartIndex = verticesStartIndex;
		verticesStartIndex += face->NumVertices;
	}

	// Sort face infos by depth
	if (sortFaces) {
		Matrix4x4 matrix;
		Matrix4x4 projMat = scene->ProjectionMatrix;
		Matrix4x4::Multiply(&matrix, &scene->ViewMatrix, &projMat);

		// Get the face depth and average the Z coordinates of
		// the faces
		for (Uint32 f = 0; f < vertexBuffer->FaceCount; f++) {
			FaceInfo* face = &vertexBuffer->FaceInfoBuffer[f];
			VertexAttribute* vertex = &vertexBuffer->Vertices[face->VerticesStartIndex];
			float depth = GL_ProjectPointDepth(&vertex[0], &matrix);
			for (Uint32 i = 1; i < face->NumVertices; i++) {
				depth += GL_ProjectPointDepth(&vertex[i], &matrix);
			}
			face->Depth = (Sint64)((depth * 0x10000) / face->NumVertices);
		}

		qsort(vertexBuffer->FaceInfoBuffer,
			vertexBuffer->FaceCount,
			sizeof(FaceInfo),
			PolygonRenderer::FaceSortFunction);
	}
}
void GL_DeleteVertexIndexList(GL_VertexBuffer* driverData) {
	for (size_t i = 0; i < driverData->VertexIndexList.size(); i++) {
		delete driverData->VertexIndexList[i];
	}
	driverData->VertexIndexList.clear();
}
void GL_UpdateVertexBuffer(Scene3D* scene,
	VertexBuffer* vertexBuffer,
	Uint32 drawMode,
	bool useBatching) {
	GL_PrepareVertexBufferUpdate(scene, vertexBuffer, drawMode);

	GL_VertexBuffer* driverData = (GL_VertexBuffer*)vertexBuffer->DriverData;
	GL_VertexBufferEntry* data = (*driverData->Entries)[0];
	GL_VertexBufferEntry* entry = data;

	driverData->Faces->clear();

	Uint32 verticesStartIndex = 0;
	Uint32 dataSplit = 0;

	vector<Uint32>* vertexIndices = nullptr;

	if (useBatching) {
		driverData->UseVertexIndices = true;

		GL_DeleteVertexIndexList(driverData);

		vertexIndices = new vector<Uint32>();
		driverData->VertexIndexList.push_back(vertexIndices);
	}

	GL_VertexBufferFace lastFace = {0};
	bool hasLastFace = false;

	for (Uint32 f = 0; f < vertexBuffer->FaceCount; f++) {
		FaceInfo* face = &vertexBuffer->FaceInfoBuffer[f];
		Uint32 vertexCount = face->NumVertices;
		int opacity = face->Blend.Opacity;
		if (!Graphics::TextureBlend) {
			opacity = 0xFF;
		}
		else if (face->Blend.Mode == BlendMode_NORMAL && opacity == 0) {
			continue;
		}

		VertexAttribute* vertex = &vertexBuffer->Vertices[face->VerticesStartIndex];
		Uint32 faceDrawMode = face->DrawMode | drawMode;

		float rgba[4] = {1.0, 1.0, 1.0, opacity / 255.0f};

		if ((faceDrawMode & DrawMode_SMOOTH_LIGHTING) == 0) {
			ColorUtils::SeparateRGB(vertex->Color, rgba);
		}

		if (useBatching) {
			// Check if the faces need to be split into
			// separate batches
			for (Uint32 v = 0; v < vertexCount; v++) {
				if (verticesStartIndex + v > GL_VertexIndexBufferMaxElements) {
					dataSplit++;
					if (dataSplit >= driverData->Entries->size()) {
						data = (GL_VertexBufferEntry*)Memory::TrackedCalloc(
							"GL_VertexBuffer::Entries",
							driverData->Capacity,
							sizeof(GL_VertexBufferEntry));
						driverData->Entries->push_back(data);
					}
					else {
						data = (*driverData->Entries)[dataSplit];
					}
					entry = data;
					verticesStartIndex = 0;
					if (driverData->UseVertexIndices) {
						vertexIndices = new vector<Uint32>();
						driverData->VertexIndexList.push_back(
							vertexIndices);
					}
					break;
				}
			}
		}

		for (Uint32 v = 0; v < vertexCount; v++) {
			entry->X = FP16_FROM(vertex->Position.X);
			entry->Y = -FP16_FROM(vertex->Position.Y);
			entry->Z = -FP16_FROM(vertex->Position.Z);

			entry->NormalX = FP16_FROM(vertex->Normal.X);
			entry->NormalY = FP16_FROM(vertex->Normal.Y);
			entry->NormalZ = FP16_FROM(vertex->Normal.Z);

			entry->TextureU = FP16_FROM(vertex->UV.X);
			entry->TextureV = FP16_FROM(vertex->UV.Y);

			if (faceDrawMode & DrawMode_SMOOTH_LIGHTING) {
				ColorUtils::SeparateRGB(vertex->Color, rgba);
			}

			entry->ColorR = rgba[0];
			entry->ColorG = rgba[1];
			entry->ColorB = rgba[2];
			entry->ColorA = rgba[3];

			vertex++;
			entry++;
		}

		GL_VertexBufferFace glFace = {0};
		glFace.VertexIndex = verticesStartIndex;
		glFace.NumVertices = vertexCount;
		glFace.UseMaterial = face->UseMaterial;
		if (glFace.UseMaterial) {
			glFace.MaterialInfo = face->MaterialInfo;
		}
		glFace.Opacity = opacity;
		glFace.BlendMode = face->Blend.Mode;
		glFace.DrawFlags =
			faceDrawMode & (DrawMode_PrimitiveMask | DrawMode_TEXTURED | DrawMode_FOG);
		glFace.PrimitiveType = GL_GetPrimitiveType(faceDrawMode);
		glFace.UseCulling = face->CullMode != FaceCull_None;
		glFace.CullMode = face->CullMode == FaceCull_Front ? GL_FRONT : GL_BACK;
		glFace.Data = data;

		if (useBatching) {
			Uint32 vtxIdx = verticesStartIndex;

			// Check if the state is consistent between
			// every face If so, the drawing process can be
			// greatly simplified
			if (driverData->UseVertexIndices) {
				if (hasLastFace) {
					if (glFace.Opacity != lastFace.Opacity ||
						glFace.BlendMode != lastFace.BlendMode ||
						glFace.DrawFlags != lastFace.DrawFlags ||
						glFace.PrimitiveType != lastFace.PrimitiveType ||
						glFace.UseCulling != lastFace.UseCulling ||
						glFace.CullMode != lastFace.CullMode) {
						driverData->UseVertexIndices = false;
					}

					if (glFace.UseMaterial != lastFace.UseMaterial) {
						driverData->UseVertexIndices = false;
					}
					else if (memcmp(&glFace.MaterialInfo,
							 &lastFace.MaterialInfo,
							 sizeof(FaceMaterial))) {
						driverData->UseVertexIndices = false;
					}
				}

				lastFace = glFace;
				hasLastFace = true;
			}

			if (glFace.PrimitiveType == GL_TRIANGLE_FAN) {
				glFace.PrimitiveType = GL_TRIANGLES;
			}

			// Make a triangle strip
			if (vertexCount > 3) {
				glFace.NumVertices = 3;

				for (unsigned i = 0; i < vertexCount - 1; i += 2) {
					glFace.VertexIndices[0] = vtxIdx + i;
					glFace.VertexIndices[1] = vtxIdx + ((i + 1) % vertexCount);
					glFace.VertexIndices[2] = vtxIdx + ((i + 2) % vertexCount);

					vertexIndices->push_back(glFace.VertexIndices[0]);
					vertexIndices->push_back(glFace.VertexIndices[1]);
					vertexIndices->push_back(glFace.VertexIndices[2]);

					driverData->Faces->push_back(glFace);
				}
			}
			else {
				glFace.VertexIndices[0] = vtxIdx;
				glFace.VertexIndices[1] = vtxIdx + 1;
				glFace.VertexIndices[2] = vtxIdx + 2;

				vertexIndices->push_back(glFace.VertexIndices[0]);
				vertexIndices->push_back(glFace.VertexIndices[1]);
				vertexIndices->push_back(glFace.VertexIndices[2]);

				driverData->Faces->push_back(glFace);
			}
		}
		else {
			driverData->Faces->push_back(glFace);
		}

		verticesStartIndex += vertexCount;
	}
}
void GL_SetVertexAttribPointers(void* vertexAtrribs, bool checkActive) {
	GLShader* shader = GLRenderer::CurrentShader;

	size_t stride = sizeof(GL_VertexBufferEntry);

	GLint active;
	if (checkActive) {
		glGetVertexAttribiv(shader->LocPosition, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &active);
		if (!active) {
			glEnableVertexAttribArray(shader->LocPosition);
		}
	}
	glVertexAttribPointer(shader->LocPosition, 3, GL_FLOAT, GL_FALSE, stride, vertexAtrribs);

	// ShaderShape3D doesn't use o_uv, so the entire attribute just
	// gets optimized out. This case is handled to prevent a
	// GL_INVALID_VALUE error.
	if (shader->LocTexCoord != -1) {
		if (checkActive) {
			glGetVertexAttribiv(
				shader->LocTexCoord, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &active);
			if (!active) {
				glEnableVertexAttribArray(shader->LocTexCoord);
			}
		}
		glVertexAttribPointer(shader->LocTexCoord,
			2,
			GL_FLOAT,
			GL_FALSE,
			stride,
			(float*)vertexAtrribs + 3);
	}

	// All shaders used for 3D rendering use o_color, so this is
	// safe to do
	if (checkActive) {
		glGetVertexAttribiv(
			shader->LocVaryingColor, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &active);
		if (!active) {
			glEnableVertexAttribArray(shader->LocVaryingColor);
		}
	}

	glVertexAttribPointer(
		shader->LocVaryingColor, 4, GL_FLOAT, GL_FALSE, stride, (float*)vertexAtrribs + 5);

	// TODO
	// glEnableVertexAttribArray(shader->LocNormal);
	// glVertexAttribPointer(shader->LocNormal, 3, GL_FLOAT,
	// GL_FALSE, stride, (float*)vertexAtrribs + 9);
}
void GL_BuildFogTable() {
	float value = Math::Clamp(1.0f - FogSmoothness, 0.0f, 1.0f);
	if (value <= 0.0) {
		for (size_t i = 0; i < 256; i++) {
			FogTable[i] = (float)i / 255.0f;
		}
		return;
	}

	float fog = 0.0f;
	float inv = 1.0f / value;

	const float recip = 1.0f / 254.0f;

	for (size_t i = 0; i < 256; i++) {
		float result = (int)(floor(fog) * value * 256.0f);
		FogTable[i] = Math::Clamp(result / 256.0f, 0.0, 1.0f);
		fog += recip * inv;
	}
}
#define SETSTATE_COMPARE_LAST(prop) \
	(!lastState || memcmp(&state.prop, &lastState->prop, sizeof(state.prop)))
#define SETSTATE_COMPARE_LAST_VAL(prop) (!lastState || state.prop != lastState->prop)
void GL_SetState(GL_State& state,
	GL_VertexBuffer* driverData,
	Matrix4x4* projMat,
	Matrix4x4* viewMat,
	GL_State* lastState = NULL) {
	bool changeShader = false;
	if (GLRenderer::CurrentShader != state.Shader) {
		GLRenderer::UseShader(state.Shader);
		changeShader = true;
		GL_SetProjectionMatrix(projMat);
		GL_SetModelViewMatrix(viewMat);
	}

	GLShader* shader = GLRenderer::CurrentShader;

	if (SETSTATE_COMPARE_LAST_VAL(VertexAtrribs)) {
		GL_SetVertexAttribPointers(state.VertexAtrribs, false);
	}

	GL_BindTexture(state.TexturePtr, GL_REPEAT);

	if (state.UsePalette) {
		GL_PreparePaletteShader();
	}

	if (shader->LocDiffuseColor != -1 &&
		(changeShader || SETSTATE_COMPARE_LAST(DiffuseColor))) {
		glUniform4f(shader->LocDiffuseColor,
			state.DiffuseColor[0],
			state.DiffuseColor[1],
			state.DiffuseColor[2],
			state.DiffuseColor[3]);
	}
	if (shader->LocSpecularColor != -1 &&
		(changeShader || SETSTATE_COMPARE_LAST(SpecularColor))) {
		glUniform4f(shader->LocSpecularColor,
			state.SpecularColor[0],
			state.SpecularColor[1],
			state.SpecularColor[2],
			state.SpecularColor[3]);
	}
	if (shader->LocAmbientColor != -1 &&
		(changeShader || SETSTATE_COMPARE_LAST(AmbientColor))) {
		glUniform4f(shader->LocAmbientColor,
			state.AmbientColor[0],
			state.AmbientColor[1],
			state.AmbientColor[2],
			state.AmbientColor[3]);
	}

	if (SETSTATE_COMPARE_LAST_VAL(CullFace)) {
		if (state.CullFace) {
			glEnable(GL_CULL_FACE);
		}
		else {
			glDisable(GL_CULL_FACE);
		}
	}

	if (SETSTATE_COMPARE_LAST_VAL(WindingOrder)) {
		glFrontFace(state.WindingOrder);
	}

	if (SETSTATE_COMPARE_LAST_VAL(DepthMask)) {
		glDepthMask(state.DepthMask);
	}

	if (SETSTATE_COMPARE_LAST_VAL(BlendMode)) {
		GL_SetBlendFuncByMode(state.BlendMode);
	}

	if (state.DrawFlags & DrawMode_FOG) {
		if (changeShader || SETSTATE_COMPARE_LAST(FogColor)) {
			glUniform4f(shader->LocFogColor,
				state.FogColor[0],
				state.FogColor[1],
				state.FogColor[2],
				state.FogColor[3]);
		}

		if (shader->LocFogLinearStart != -1 &&
			(changeShader || SETSTATE_COMPARE_LAST_VAL(FogParams[0]))) {
			glUniform1f(shader->LocFogLinearStart, state.FogParams[0]);
		}
		if (shader->LocFogLinearEnd != -1 &&
			(changeShader || SETSTATE_COMPARE_LAST_VAL(FogParams[1]))) {
			glUniform1f(shader->LocFogLinearEnd, state.FogParams[1]);
		}
		if (shader->LocFogDensity != -1 &&
			(changeShader || SETSTATE_COMPARE_LAST_VAL(FogParams[2]))) {
			glUniform1f(shader->LocFogDensity, state.FogParams[2]);
		}

		bool fogChanged = false;
		if (state.FogParams[3] != FogSmoothness) {
			FogSmoothness = state.FogParams[3];
			fogChanged = true;
			GL_BuildFogTable();
		}

		if (changeShader || fogChanged) {
			glUniform1fv(shader->LocFogTable, 256, FogTable);
		}
	}
}
#undef SETSTATE_COMPARE_LAST
#undef SETSTATE_COMPARE_LAST_VAL
void GL_UpdateStateFromFace(GL_State& state,
	GL_VertexBufferFace& face,
	Scene3D* scene,
	GLenum cullWindingOrder) {
	bool fogEnabled = face.DrawFlags & DrawMode_FOG;
	if (fogEnabled) {
		state.FogMode = scene->Fog.Equation;

		state.FogColor[0] = scene->Fog.Color.R;
		state.FogColor[1] = scene->Fog.Color.G;
		state.FogColor[2] = scene->Fog.Color.B;
		state.FogColor[3] = 1.0f;

		state.FogParams[0] = scene->Fog.Start;
		state.FogParams[1] = scene->Fog.End;
		state.FogParams[2] = scene->Fog.Density * scene->Fog.Density;
		state.FogParams[3] = scene->Fog.Smoothness;
	}

	state.UseMaterial = false;
	state.UseTexture = false;
	state.UsePalette = false;

	if (face.UseMaterial) {
		state.UseMaterial = true;

		state.DiffuseColor[0] = (float)face.MaterialInfo.Diffuse[0] / 0x100;
		state.DiffuseColor[1] = (float)face.MaterialInfo.Diffuse[1] / 0x100;
		state.DiffuseColor[2] = (float)face.MaterialInfo.Diffuse[2] / 0x100;
		state.DiffuseColor[3] = (float)face.MaterialInfo.Diffuse[3] / 0x100;

		state.SpecularColor[0] = (float)face.MaterialInfo.Specular[0] / 0x100;
		state.SpecularColor[1] = (float)face.MaterialInfo.Specular[1] / 0x100;
		state.SpecularColor[2] = (float)face.MaterialInfo.Specular[2] / 0x100;
		state.SpecularColor[3] = (float)face.MaterialInfo.Specular[3] / 0x100;

		state.AmbientColor[0] = (float)face.MaterialInfo.Ambient[0] / 0x100;
		state.AmbientColor[1] = (float)face.MaterialInfo.Ambient[1] / 0x100;
		state.AmbientColor[2] = (float)face.MaterialInfo.Ambient[2] / 0x100;
		state.AmbientColor[3] = (float)face.MaterialInfo.Ambient[3] / 0x100;

		if (face.DrawFlags & DrawMode_TEXTURED) {
			state.TexturePtr = (Texture*)face.MaterialInfo.Texture;
			state.UseTexture = state.TexturePtr != nullptr;
			state.UsePalette = state.UseTexture && Graphics::UsePalettes &&
				state.TexturePtr->Paletted;
		}
	}
	else {
		state.DiffuseColor[0] = state.DiffuseColor[1] = state.DiffuseColor[2] =
			state.DiffuseColor[3] = 1.0f;

		state.SpecularColor[0] = state.SpecularColor[1] = state.SpecularColor[2] =
			state.SpecularColor[3] = 1.0f;

		state.AmbientColor[0] = state.AmbientColor[1] = state.AmbientColor[2] =
			state.AmbientColor[3] = 1.0f;
	}

	if (!state.UseTexture) {
		state.TexturePtr = nullptr;
	}

	if (fogEnabled) {
		switch (scene->Fog.Equation) {
		case FogEquation_Exp:
			state.Shader =
				GLRenderer::ShaderFogExp->Get(state.UseTexture, state.UsePalette);
			break;
		default:
			state.Shader = GLRenderer::ShaderFogLinear->Get(
				state.UseTexture, state.UsePalette);
			break;
		}
	}
	else {
		state.Shader = GLRenderer::ShaderShape3D->Get(state.UseTexture);
	}

	if (face.UseCulling) {
		state.CullFace = true;
		state.WindingOrder = cullWindingOrder;
	}
	else {
		state.CullFace = false;
		state.WindingOrder = GL_CCW;
	}

	state.CullMode = face.CullMode;
	state.DepthMask = face.Opacity == 0xFF ? GL_TRUE : GL_FALSE;
	state.BlendMode = face.BlendMode;
	state.DrawFlags = face.DrawFlags;
	state.PrimitiveType = face.PrimitiveType;
	state.VertexAtrribs = face.Data;
}
void GL_DrawBatchedScene3D(GL_VertexBuffer* driverData,
	vector<Uint32>* vertexIndices,
	GLenum primitiveType,
	bool remakeVtxIdxBuf) {
	size_t numIndices = vertexIndices->size();

	size_t capacity = numIndices * GL_VertexIndexBufferStride;
	if (driverData->VertexIndexBuffer == nullptr) {
		driverData->VertexIndexBuffer = (void*)Memory::Malloc(capacity);
		driverData->VertexIndexBufferCapacity = capacity;
		remakeVtxIdxBuf = true;
	}
	else if (capacity > driverData->VertexIndexBufferCapacity) {
		driverData->VertexIndexBuffer =
			(void*)Memory::Realloc(driverData->VertexIndexBuffer, capacity);
		driverData->VertexIndexBufferCapacity = capacity;
		remakeVtxIdxBuf = true;
	}

	// This can take enough time to matter with buffers that have a
	// lot of vertices, so this is only done when needed.
	if (remakeVtxIdxBuf) {
		if (GL_VertexIndexBufferFormat == GL_UNSIGNED_BYTE) {
			Uint8* buf = (Uint8*)driverData->VertexIndexBuffer;
			for (size_t i = 0; i < numIndices; i++) {
				buf[i] = (*vertexIndices)[i];
			}
		}
		else if (GL_VertexIndexBufferFormat == GL_UNSIGNED_SHORT) {
			Uint16* buf = (Uint16*)driverData->VertexIndexBuffer;
			for (size_t i = 0; i < numIndices; i++) {
				buf[i] = (*vertexIndices)[i];
			}
		}
		else {
			memcpy(driverData->VertexIndexBuffer,
				vertexIndices->data(),
				sizeof(Uint32) * numIndices);
		}
	}

	glDrawElements(primitiveType,
		numIndices,
		GL_VertexIndexBufferFormat,
		(const void*)driverData->VertexIndexBuffer);
	CHECK_GL();
}
PolygonRenderer* GL_GetPolygonRenderer() {
	if (!polyRenderer.SetBuffers()) {
		return nullptr;
	}

	polyRenderer.DrawMode = polyRenderer.ScenePtr ? polyRenderer.ScenePtr->DrawMode : 0;
	polyRenderer.FaceCullMode =
		polyRenderer.ScenePtr ? polyRenderer.ScenePtr->FaceCullMode : FaceCull_None;
	polyRenderer.CurrentColor = ColorUtils::ToRGB(Graphics::BlendColors);

	GL_VertexBuffer* driverData = (GL_VertexBuffer*)polyRenderer.VertexBuf->DriverData;
	driverData->Changed = true;

	return &polyRenderer;
}

// Initialization and disposal functions
void GLRenderer::Init() {
	Graphics::SupportsBatching = true;
	Graphics::PreferredPixelFormat = SDL_PIXELFORMAT_ABGR8888;

	Log::Print(Log::LOG_INFO, "Renderer: OpenGL");

#ifndef MACOSX
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

#ifdef GL_SUPPORTS_MULTISAMPLING
	if (Graphics::MultisamplingEnabled) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, Graphics::MultisamplingEnabled);
	}
#endif

	if (Application::Platform == Platforms::iOS) {
		SDL_GL_SetAttribute(SDL_GL_RETAINED_BACKING, 0);
	}

	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	Context = SDL_GL_CreateContext(Application::Window);
	CHECK_GL();
	if (!Context) {
		Log::Print(Log::LOG_ERROR, "Could not create OpenGL context: %s", SDL_GetError());
		exit(-1);
	}

#ifdef USING_GLEW
	glewExperimental = GL_TRUE;
	GLenum res = glewInit();
	if (res != GLEW_OK && res != GLEW_ERROR_NO_GLX_DISPLAY) {
		Log::Print(Log::LOG_ERROR,
			"Could not create GLEW context: %s",
			glewGetErrorString(res));
		exit(-1);
	}
#endif

	if (Graphics::VsyncEnabled) {
		GLRenderer::SetVSync(true);
	}

	int max, w, h, ww, wh;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);

	Graphics::MaxTextureWidth = max;
	Graphics::MaxTextureHeight = max;

	SDL_GL_GetDrawableSize(Application::Window, &w, &h);
	SDL_GetWindowSize(Application::Window, &ww, &wh);

	RetinaScale = 1.0;
	if (h > wh) {
		RetinaScale = h / wh;
	}

	Log::Print(Log::LOG_INFO, "OpenGL Version: %s", glGetString(GL_VERSION));
	Log::Print(Log::LOG_INFO, "GLSL Version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
	Log::Print(Log::LOG_INFO,
		"Graphics Card: %s %s",
		glGetString(GL_VENDOR),
		glGetString(GL_RENDERER));
	Log::Print(Log::LOG_INFO, "Drawable Size: %d x %d", w, h);

	if (Application::Platform == Platforms::iOS ||
		Application::Platform == Platforms::Android) {
		UseDepthTesting = false;
	}

	// Enable/Disable GL features
	glEnable(GL_BLEND);
	GLRenderer::SetDepthTesting(Graphics::UseDepthTesting);
	glDepthMask(GL_TRUE);

#ifdef GL_SUPPORTS_MULTISAMPLING
	if (Graphics::MultisamplingEnabled) {
		glEnable(GL_MULTISAMPLE);
	}
#endif

	glBlendFuncSeparate(
		GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
	glDepthFunc(GL_LEQUAL);

#ifdef GL_SUPPORTS_SMOOTHING
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
#endif

#ifdef USE_USHORT_VTXBUFFER
	GL_VertexIndexBufferFormat = GL_UNSIGNED_SHORT;
	GL_VertexIndexBufferMaxElements = 0xFFFF;
	GL_VertexIndexBufferStride = sizeof(Uint16);
#else
	if (Application::Platform == Platforms::iOS ||
		Application::Platform == Platforms::Android) {
		GL_VertexIndexBufferFormat = GL_UNSIGNED_SHORT;
		GL_VertexIndexBufferMaxElements = 0xFFFF;
		GL_VertexIndexBufferStride = sizeof(Uint16);
	}
	else {
		GL_VertexIndexBufferFormat = GL_UNSIGNED_INT;
		GL_VertexIndexBufferMaxElements = 0xFFFFFFFF;
		GL_VertexIndexBufferStride = sizeof(Uint32);
	}
#endif

	GL_MakeShaders();
	GL_MakeShapeBuffers();

	UseShader(ShaderShape->Get());
	glEnableVertexAttribArray(GLRenderer::CurrentShader->LocPosition);

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &DefaultFramebuffer);
#ifdef GL_SUPPORTS_RENDERBUFFER
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &DefaultRenderbuffer);
#endif

	Log::Print(Log::LOG_INFO, "Default Framebuffer: %d", DefaultFramebuffer);
	Log::Print(Log::LOG_INFO, "Default Renderbuffer: %d", DefaultRenderbuffer);

	glClearColor(0.0, 0.0, 0.0, 0.0);
}
Uint32 GLRenderer::GetWindowFlags() {
#ifdef GL_SUPPORTS_MULTISAMPLING
	if (Graphics::MultisamplingEnabled) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, Graphics::MultisamplingEnabled);
	}
#endif

	return SDL_WINDOW_OPENGL;
}
void GLRenderer::SetVSync(bool enabled) {
	if (enabled) {
		if (SDL_GL_SetSwapInterval(1) < 0) {
			Log::Print(Log::LOG_WARN, "Could not enable V-Sync: %s", SDL_GetError());
			enabled = false;
		}
	}
	else if (SDL_GL_SetSwapInterval(0) < 0) {
		Log::Print(Log::LOG_WARN, "Could not disable V-Sync: %s", SDL_GetError());
	}
	Graphics::VsyncEnabled = enabled;
}
void GLRenderer::SetGraphicsFunctions() {
	Graphics::PixelOffset = 0.0f;

	Graphics::Internal.Init = GLRenderer::Init;
	Graphics::Internal.GetWindowFlags = GLRenderer::GetWindowFlags;
	Graphics::Internal.SetVSync = GLRenderer::SetVSync;
	Graphics::Internal.Dispose = GLRenderer::Dispose;

	// Texture management functions
	Graphics::Internal.CreateTexture = GLRenderer::CreateTexture;
	Graphics::Internal.LockTexture = GLRenderer::LockTexture;
	Graphics::Internal.UpdateTexture = GLRenderer::UpdateTexture;
	Graphics::Internal.UpdateYUVTexture = GLRenderer::UpdateTextureYUV;
	Graphics::Internal.UnlockTexture = GLRenderer::UnlockTexture;
	Graphics::Internal.DisposeTexture = GLRenderer::DisposeTexture;

	// Viewport and view-related functions
	Graphics::Internal.SetRenderTarget = GLRenderer::SetRenderTarget;
	Graphics::Internal.ReadFramebuffer = GLRenderer::ReadFramebuffer;
	Graphics::Internal.UpdateWindowSize = GLRenderer::UpdateWindowSize;
	Graphics::Internal.UpdateViewport = GLRenderer::UpdateViewport;
	Graphics::Internal.UpdateClipRect = GLRenderer::UpdateClipRect;
	Graphics::Internal.UpdateOrtho = GLRenderer::UpdateOrtho;
	Graphics::Internal.UpdatePerspective = GLRenderer::UpdatePerspective;
	Graphics::Internal.UpdateProjectionMatrix = GLRenderer::UpdateProjectionMatrix;
	Graphics::Internal.MakePerspectiveMatrix = GLRenderer::MakePerspectiveMatrix;

	// Shader-related functions
	Graphics::Internal.UseShader = GLRenderer::UseShader;
	Graphics::Internal.SetUniformF = GLRenderer::SetUniformF;
	Graphics::Internal.SetUniformI = GLRenderer::SetUniformI;
	Graphics::Internal.SetUniformTexture = GLRenderer::SetUniformTexture;

	// Palette-related functions
	Graphics::Internal.UpdateGlobalPalette = GLRenderer::UpdateGlobalPalette;

	// These guys
	Graphics::Internal.Clear = GLRenderer::Clear;
	Graphics::Internal.Present = GLRenderer::Present;

	// Draw mode setting functions
	Graphics::Internal.SetBlendColor = GLRenderer::SetBlendColor;
	Graphics::Internal.SetBlendMode = GLRenderer::SetBlendMode;
	Graphics::Internal.SetTintColor = GLRenderer::SetTintColor;
	Graphics::Internal.SetTintMode = GLRenderer::SetTintMode;
	Graphics::Internal.SetTintEnabled = GLRenderer::SetTintEnabled;
	Graphics::Internal.SetLineWidth = GLRenderer::SetLineWidth;

	// Primitive drawing functions
	Graphics::Internal.StrokeLine = GLRenderer::StrokeLine;
	Graphics::Internal.StrokeCircle = GLRenderer::StrokeCircle;
	Graphics::Internal.StrokeEllipse = GLRenderer::StrokeEllipse;
	Graphics::Internal.StrokeRectangle = GLRenderer::StrokeRectangle;
	Graphics::Internal.FillCircle = GLRenderer::FillCircle;
	Graphics::Internal.FillEllipse = GLRenderer::FillEllipse;
	Graphics::Internal.FillTriangle = GLRenderer::FillTriangle;
	Graphics::Internal.FillRectangle = GLRenderer::FillRectangle;

	// Texture drawing functions
	Graphics::Internal.DrawTexture = GLRenderer::DrawTexture;
	Graphics::Internal.DrawSprite = GLRenderer::DrawSprite;
	Graphics::Internal.DrawSpritePart = GLRenderer::DrawSpritePart;

	// 3D drawing functions
	Graphics::Internal.DrawPolygon3D = GLRenderer::DrawPolygon3D;
	Graphics::Internal.DrawSceneLayer3D = GLRenderer::DrawSceneLayer3D;
	Graphics::Internal.DrawModel = GLRenderer::DrawModel;
	Graphics::Internal.DrawModelSkinned = GLRenderer::DrawModelSkinned;
	Graphics::Internal.DrawVertexBuffer = GLRenderer::DrawVertexBuffer;
	Graphics::Internal.BindVertexBuffer = GLRenderer::BindVertexBuffer;
	Graphics::Internal.UnbindVertexBuffer = GLRenderer::UnbindVertexBuffer;
	Graphics::Internal.ClearScene3D = GLRenderer::ClearScene3D;
	Graphics::Internal.DrawScene3D = GLRenderer::DrawScene3D;

	Graphics::Internal.CreateVertexBuffer = GLRenderer::CreateVertexBuffer;
	Graphics::Internal.DeleteVertexBuffer = GLRenderer::DeleteVertexBuffer;
	Graphics::Internal.MakeFrameBufferID = GLRenderer::MakeFrameBufferID;
	Graphics::Internal.DeleteFrameBufferID = GLRenderer::DeleteFrameBufferID;

	Graphics::Internal.SetDepthTesting = GLRenderer::SetDepthTesting;
}
void GLRenderer::Dispose() {
	glDeleteBuffers(1, &BufferCircleFill);
	glDeleteBuffers(1, &BufferCircleStroke);
	glDeleteBuffers(1, &BufferSquareFill);

	delete ShaderShape;
	delete ShaderShape3D;
	delete ShaderFogLinear;
	delete ShaderFogExp;
	delete ShaderYUV;

	SDL_GL_DeleteContext(Context);
}

// Texture management functions
Texture* GLRenderer::CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
	Texture* texture = Texture::New(format, access, width, height);
	texture->DriverData =
		Memory::TrackedCalloc("Texture::DriverData", 1, sizeof(GL_TextureData));

	GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;

	textureData->TextureTarget = GL_TEXTURE_2D;

	textureData->TextureStorageFormat = GL_RGBA;
	textureData->PixelDataFormat = GL_RGBA;
	textureData->PixelDataType = GL_UNSIGNED_BYTE;

	textureData->Accessed = false;

	// Set format
	switch (texture->Format) {
	case SDL_PIXELFORMAT_YV12:
	case SDL_PIXELFORMAT_IYUV:
	case SDL_PIXELFORMAT_NV12:
	case SDL_PIXELFORMAT_NV21:
		textureData->TextureStorageFormat = GL_MONOCHROME_PIXELFORMAT;
		textureData->PixelDataFormat = GL_MONOCHROME_PIXELFORMAT;
		textureData->PixelDataType = GL_UNSIGNED_BYTE;
		break;
	default:
		break;
	}

	// Set texture access
	switch (texture->Access) {
	case SDL_TEXTUREACCESS_TARGET: {
		textureData->Framebuffer = true;
		glGenFramebuffers(1, &textureData->FBO);

#ifdef GL_SUPPORTS_RENDERBUFFER
		glGenRenderbuffers(1, &textureData->RBO);
		glBindRenderbuffer(GL_RENDERBUFFER, textureData->RBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
		CHECK_GL();
#endif

#ifdef GL_SUPPORTS_MULTISAMPLING
// textureData->TextureTarget = GL_TEXTURE_2D_MULTISAMPLE;
#endif

		width *= RetinaScale;
		height *= RetinaScale;
		break;
	}
	case SDL_TEXTUREACCESS_STREAMING: {
		texture->Pitch = texture->Width * SDL_BYTESPERPIXEL(texture->Format);

		size_t size = texture->Pitch * texture->Height;
		if (texture->Format == SDL_PIXELFORMAT_YV12 ||
			texture->Format == SDL_PIXELFORMAT_IYUV) {
			// Need to add size for the U and V planes.
			size += 2 * ((texture->Height + 1) / 2) * ((texture->Pitch + 1) / 2);
		}
		if (texture->Format == SDL_PIXELFORMAT_NV12 ||
			texture->Format == SDL_PIXELFORMAT_NV21) {
			// Need to add size for the U/V plane.
			size += 2 * ((texture->Height + 1) / 2) * ((texture->Pitch + 1) / 2);
		}
		texture->Pixels = calloc(1, size);
		break;
	}
	}

	// Generate texture buffer
	glGenTextures(1, &textureData->TextureID);
	glBindTexture(textureData->TextureTarget, textureData->TextureID);

	// Set target
	switch (textureData->TextureTarget) {
	case GL_TEXTURE_2D:
		glTexImage2D(textureData->TextureTarget,
			0,
			textureData->TextureStorageFormat,
			width,
			height,
			0,
			textureData->PixelDataFormat,
			textureData->PixelDataType,
			0);
		CHECK_GL();
		break;
		{
#ifdef GL_SUPPORTS_MULTISAMPLING
		case GL_TEXTURE_2D_MULTISAMPLE:
			glTexImage2DMultisample(textureData->TextureTarget,
				Graphics::MultisamplingEnabled,
				textureData->PixelDataFormat,
				width,
				height,
				GL_TRUE);
			CHECK_GL();
			break;
#endif
		}
	default:
		Log::Print(Log::LOG_ERROR, "Unsupported GL texture target!");
		break;
	}

	// Set texture filter
	GLenum textureFilter = GL_NEAREST;
	if (Graphics::TextureInterpolate) {
		textureFilter = GL_LINEAR;
	}
	glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MAG_FILTER, textureFilter);
	glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MIN_FILTER, textureFilter);

	if (texture->Format == SDL_PIXELFORMAT_YV12 || texture->Format == SDL_PIXELFORMAT_IYUV) {
		textureData->YUV = true;

		glGenTextures(1, &textureData->TextureU);
		glGenTextures(1, &textureData->TextureV);

		glBindTexture(textureData->TextureTarget, textureData->TextureU);
		glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MAG_FILTER, textureFilter);
		glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MIN_FILTER, textureFilter);
		glTexImage2D(textureData->TextureTarget,
			0,
			textureData->TextureStorageFormat,
			(width + 1) / 2,
			(height + 1) / 2,
			0,
			textureData->PixelDataFormat,
			textureData->PixelDataType,
			NULL);
		CHECK_GL();

		glBindTexture(textureData->TextureTarget, textureData->TextureV);
		glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MAG_FILTER, textureFilter);
		glTexParameteri(textureData->TextureTarget, GL_TEXTURE_MIN_FILTER, textureFilter);
		glTexImage2D(textureData->TextureTarget,
			0,
			textureData->TextureStorageFormat,
			(width + 1) / 2,
			(height + 1) / 2,
			0,
			textureData->PixelDataFormat,
			textureData->PixelDataType,
			NULL);
		CHECK_GL();
	}

	glBindTexture(textureData->TextureTarget, 0);

	texture->ID = textureData->TextureID;
	Graphics::TextureMap->Put(texture->ID, texture);

	return texture;
}
int GLRenderer::LockTexture(Texture* texture, void** pixels, int* pitch) {
	return 0;
}
int GLRenderer::UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch) {
	Uint32 inputPixelsX = 0;
	Uint32 inputPixelsY = 0;
	Uint32 inputPixelsW = texture->Width;
	Uint32 inputPixelsH = texture->Height;
	if (src) {
		inputPixelsX = src->x;
		inputPixelsY = src->y;
		inputPixelsW = src->w;
		inputPixelsH = src->h;
	}

	if (Graphics::NoInternalTextures) {
		if (inputPixelsW > Graphics::MaxTextureWidth) {
			inputPixelsW = Graphics::MaxTextureWidth;
		}
		if (inputPixelsH > Graphics::MaxTextureHeight) {
			inputPixelsH = Graphics::MaxTextureHeight;
		}
	}

	GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;

	textureData->TextureStorageFormat = GL_RGBA;
	textureData->PixelDataFormat = GL_RGBA;
	textureData->PixelDataType = GL_UNSIGNED_BYTE;

	glBindTexture(textureData->TextureTarget, textureData->TextureID);
	glTexSubImage2D(textureData->TextureTarget,
		0,
		inputPixelsX,
		inputPixelsY,
		inputPixelsW,
		inputPixelsH,
		textureData->PixelDataFormat,
		textureData->PixelDataType,
		pixels);
	CHECK_GL();
	return 0;
}
int GLRenderer::UpdateTextureYUV(Texture* texture,
	SDL_Rect* src,
	void* pixelsY,
	int pitchY,
	void* pixelsU,
	int pitchU,
	void* pixelsV,
	int pitchV) {
	int inputPixelsX = 0;
	int inputPixelsY = 0;
	int inputPixelsW = texture->Width;
	int inputPixelsH = texture->Height;
	if (src) {
		inputPixelsX = src->x;
		inputPixelsY = src->y;
		inputPixelsW = src->w;
		inputPixelsH = src->h;
	}

	GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;

	glBindTexture(textureData->TextureTarget, textureData->TextureID);
	glTexSubImage2D(textureData->TextureTarget,
		0,
		inputPixelsX,
		inputPixelsY,
		inputPixelsW,
		inputPixelsH,
		textureData->PixelDataFormat,
		textureData->PixelDataType,
		pixelsY);
	CHECK_GL();

	inputPixelsX = inputPixelsX / 2;
	inputPixelsY = inputPixelsY / 2;
	inputPixelsW = (inputPixelsW + 1) / 2;
	inputPixelsH = (inputPixelsH + 1) / 2;

	glBindTexture(textureData->TextureTarget,
		texture->Format != SDL_PIXELFORMAT_YV12 ? textureData->TextureV
							: textureData->TextureU);

	glTexSubImage2D(textureData->TextureTarget,
		0,
		inputPixelsX,
		inputPixelsY,
		inputPixelsW,
		inputPixelsH,
		textureData->PixelDataFormat,
		textureData->PixelDataType,
		pixelsU);
	CHECK_GL();

	glBindTexture(textureData->TextureTarget,
		texture->Format != SDL_PIXELFORMAT_YV12 ? textureData->TextureU
							: textureData->TextureV);

	glTexSubImage2D(textureData->TextureTarget,
		0,
		inputPixelsX,
		inputPixelsY,
		inputPixelsW,
		inputPixelsH,
		textureData->PixelDataFormat,
		textureData->PixelDataType,
		pixelsV);
	CHECK_GL();
	return 0;
}
void GLRenderer::UnlockTexture(Texture* texture) {}
void GLRenderer::DisposeTexture(Texture* texture) {
	GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;
	if (!textureData) {
		return;
	}

	if (texture->Access == SDL_TEXTUREACCESS_TARGET) {
		glDeleteFramebuffers(1, &textureData->FBO);
#ifdef GL_SUPPORTS_RENDERBUFFER
		glDeleteRenderbuffers(1, &textureData->RBO);
#endif
	}
	else if (texture->Access == SDL_TEXTUREACCESS_STREAMING) {
		// free(texture->Pixels);
	}
	if (textureData->YUV) {
		glDeleteTextures(1, &textureData->TextureU);
		glDeleteTextures(1, &textureData->TextureV);
	}
	if (textureData->TextureID) {
		glDeleteTextures(1, &textureData->TextureID);
	}
	Memory::Free(textureData);
}

// Viewport and view-related functions
void GLRenderer::SetRenderTarget(Texture* texture) {
	if (texture == NULL) {
		glBindFramebuffer(GL_FRAMEBUFFER, DefaultFramebuffer);

#ifdef GL_SUPPORTS_RENDERBUFFER
		glBindRenderbuffer(GL_RENDERBUFFER, DefaultRenderbuffer);
#endif
	}
	else {
		GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;
		if (!textureData->Framebuffer) {
			Log::Print(Log::LOG_WARN, "Cannot render to non-framebuffer texture!");
			return;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, textureData->FBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0,
			textureData->TextureTarget,
			textureData->TextureID,
			0);

#ifdef GL_SUPPORTS_RENDERBUFFER
		glBindRenderbuffer(GL_RENDERBUFFER, textureData->RBO);
		glFramebufferRenderbuffer(
			GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, textureData->RBO);
#endif

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			Log::Print(Log::LOG_ERROR, "glFramebufferTexture2D() failed: ");
			CHECK_GL();
		}
	}
}
void GLRenderer::ReadFramebuffer(void* pixels, int width, int height) {
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	if (Graphics::CurrentRenderTarget) {
		return;
	}

	Uint32* data = (Uint32*)pixels;
	Uint32* temp = new Uint32[width];

	for (int i = 0; i < height / 2; i++) {
		memcpy(temp, &data[i * width], width * 4);
		memcpy(&data[i * width], &data[(height - 1 - i) * width], width * 4);
		memcpy(&data[(height - 1 - i) * width], temp, width * 4);
	}

	delete[] temp;
}
void GLRenderer::UpdateWindowSize(int width, int height) {
	GLRenderer::UpdateViewport();
}
void GLRenderer::UpdateViewport() {
	Viewport* vp = &Graphics::CurrentViewport;
	if (Graphics::CurrentRenderTarget) {
		glViewport(vp->X * RetinaScale,
			vp->Y * RetinaScale,
			vp->Width * RetinaScale,
			vp->Height * RetinaScale);
	}
	else {
		int h;
		SDL_GetWindowSize(Application::Window, NULL, &h);
		glViewport(vp->X * RetinaScale,
			(h - vp->Y - vp->Height) * RetinaScale,
			vp->Width * RetinaScale,
			vp->Height * RetinaScale);
	}

	// NOTE: According to SDL2 we should be setting projection
	// matrix here. GLRenderer::UpdateOrtho(vp->Width, vp->Height);
	GLRenderer::UpdateProjectionMatrix();
}
void GLRenderer::UpdateClipRect() {
	ClipArea clip = Graphics::CurrentClip;
	if (Graphics::CurrentClip.Enabled) {
		Viewport view = Graphics::CurrentViewport;

		if (!GL_ClippingEnabled) {
			glEnable(GL_SCISSOR_TEST);
		}

		if (Graphics::CurrentRenderTarget) {
			glScissor((view.X + clip.X) * RetinaScale,
				(view.Y + clip.Y) * RetinaScale,
				(clip.Width) * RetinaScale,
				(clip.Height) * RetinaScale);
		}
		else {
			int w, h;
			float scaleW = RetinaScale, scaleH = RetinaScale;
			SDL_GetWindowSize(Application::Window, &w, &h);

			View* currentView = Graphics::CurrentView;
			scaleW *= w / currentView->Width;
			scaleH *= h / currentView->Height;

			glScissor((view.X + clip.X) * scaleW,
				h * RetinaScale - (view.Y + clip.Y + clip.Height) * scaleH,
				(clip.Width) * scaleW,
				(clip.Height) * scaleH);
		}

		GL_ClippingEnabled = true;
	}
	else if (GL_ClippingEnabled) {
		glDisable(GL_SCISSOR_TEST);
		GL_ClippingEnabled = false;
	}
}
void GLRenderer::UpdateOrtho(float left, float top, float right, float bottom) {
	// if (Graphics::CurrentRenderTarget)
	//     Matrix4x4::Ortho(Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix,
	//     left, right, bottom, top, -500.0f, 500.0f);
	// else
	Matrix4x4::Ortho(Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix,
		left,
		right,
		top,
		bottom,
		-500.0f,
		500.0f);

	Matrix4x4::Copy(Scene::Views[Scene::ViewCurrent].ProjectionMatrix,
		Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix);
}
void GLRenderer::UpdatePerspective(float fovy, float aspect, float nearv, float farv) {
	MakePerspectiveMatrix(
		Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix, fovy, nearv, farv, aspect);
	Matrix4x4::Copy(Scene::Views[Scene::ViewCurrent].ProjectionMatrix,
		Scene::Views[Scene::ViewCurrent].BaseProjectionMatrix);
}
void GLRenderer::UpdateProjectionMatrix() {}
void GLRenderer::MakePerspectiveMatrix(Matrix4x4* out,
	float fov,
	float near,
	float far,
	float aspect) {
	float f = 1.0f / tanf(fov / 2.0f);
	float delta = far - near;

	f *= 2.0f;

	out->Values[0] = f / aspect;
	out->Values[1] = 0.0f;
	out->Values[2] = 0.0f;
	out->Values[3] = 0.0f;

	out->Values[4] = 0.0f;
	out->Values[5] = -f;
	out->Values[6] = 0.0f;
	out->Values[7] = 0.0f;

	out->Values[8] = 0.0f;
	out->Values[9] = 0.0f;
	out->Values[10] = -near / delta;
	out->Values[11] = -1.0f;

	out->Values[12] = 0.0f;
	out->Values[13] = 0.0f;
	out->Values[14] = -(near * far) / delta;
	out->Values[15] = 0.0f;
}

// Shader-related functions
void GLRenderer::UseShader(void* shader) {
	if (GLRenderer::CurrentShader != (GLShader*)shader) {
		if (GLRenderer::CurrentShader) {
			if (GLRenderer::CurrentShader->LocPosition != -1) {
				glDisableVertexAttribArray(GLRenderer::CurrentShader->LocPosition);
			}
			if (GLRenderer::CurrentShader->LocTexCoord != -1) {
				glDisableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord);
			}
			if (GLRenderer::CurrentShader->LocVaryingColor != -1) {
				glDisableVertexAttribArray(
					GLRenderer::CurrentShader->LocVaryingColor);
			}
		}

		GLRenderer::CurrentShader = (GLShader*)shader;

		GLRenderer::CurrentShader->Use();

		if (GLRenderer::CurrentShader->LocPosition != -1) {
			glEnableVertexAttribArray(GLRenderer::CurrentShader->LocPosition);
		}
		if (GLRenderer::CurrentShader->LocTexCoord != -1) {
			glEnableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord);
		}
		if (GLRenderer::CurrentShader->LocVaryingColor != -1) {
			glEnableVertexAttribArray(GLRenderer::CurrentShader->LocVaryingColor);
		}

		if (GL_ActiveTexture != GL_TEXTURE0) {
			glActiveTexture(GL_ActiveTexture = GL_TEXTURE0);
		}
		glUniform1i(GLRenderer::CurrentShader->LocTexture, 0);
	}
}
void GLRenderer::SetUniformF(int location, int count, float* values) {
	switch (count) {
	case 1:
		glUniform1f(location, values[0]);
		break;
	case 2:
		glUniform2f(location, values[0], values[1]);
		break;
	case 3:
		glUniform3f(location, values[0], values[1], values[2]);
		break;
	case 4:
		glUniform4f(location, values[0], values[1], values[2], values[3]);
		break;
	}
}
void GLRenderer::SetUniformI(int location, int count, int* values) {
	glUniform1iv(location, count, values);
}
void GLRenderer::SetUniformTexture(Texture* texture, int uniform_index, int slot) {
	GL_TextureData* textureData = (GL_TextureData*)texture->DriverData;
	glActiveTexture(GL_ActiveTexture = GL_TEXTURE0 + slot);
	glUniform1i(uniform_index, slot);
	glBindTexture(GL_TEXTURE_2D, textureData->TextureID);
}

// Palette-related functions
void GLRenderer::UpdateGlobalPalette() {}

// These guys
void GLRenderer::Clear() {
	if (UseDepthTesting) {
#ifdef GL_ES
		glClearDepthf(1.0f);
#else
		glClearDepth(1.0f);
#endif
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else {
		glClear(GL_COLOR_BUFFER_BIT);
	}
}
void GLRenderer::Present() {
	SDL_GL_SwapWindow(Application::Window);
	CHECK_GL();
}

// Draw mode setting functions
void GLRenderer::SetBlendColor(float r, float g, float b, float a) {}
void GLRenderer::SetBlendMode(int srcC, int dstC, int srcA, int dstA) {
	glBlendFuncSeparate(GL_GetBlendFactorFromHatchEnum(srcC),
		GL_GetBlendFactorFromHatchEnum(dstC),
		GL_GetBlendFactorFromHatchEnum(srcA),
		GL_GetBlendFactorFromHatchEnum(dstA));
}
void GLRenderer::SetTintColor(float r, float g, float b, float a) {}
void GLRenderer::SetTintMode(int mode) {}
void GLRenderer::SetTintEnabled(bool enabled) {}
void GLRenderer::SetLineWidth(float n) {
	glLineWidth(n);
}

// Primitive drawing functions
void GLRenderer::StrokeLine(float x1, float y1, float x2, float y2) {
	// Graphics::Save();
	GL_Predraw(NULL);

	float v[6];
	v[0] = x1;
	v[1] = y1;
	v[2] = 0.0f;
	v[3] = x2;
	v[4] = y2;
	v[5] = 0.0f;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glVertexAttribPointer(CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, v);
	glDrawArrays(GL_LINES, 0, 2);
	CHECK_GL();
	// Graphics::Restore();
}
void GLRenderer::StrokeCircle(float x, float y, float rad, float thickness) {
	Graphics::Save();
	Graphics::Translate(x, y, 0.0f);
	Graphics::Scale(rad, rad, 1.0f);
	GL_Predraw(NULL);

	glBindBuffer(GL_ARRAY_BUFFER, BufferCircleStroke);
	glVertexAttribPointer(CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_LINE_STRIP, 0, 361);
	CHECK_GL();
	Graphics::Restore();
}
void GLRenderer::StrokeEllipse(float x, float y, float w, float h) {
	Graphics::Save();
	Graphics::Translate(x + w / 2, y + h / 2, 0.0f);
	Graphics::Scale(w / 2, h / 2, 1.0f);
	GL_Predraw(NULL);

	glBindBuffer(GL_ARRAY_BUFFER, BufferCircleStroke);
	glVertexAttribPointer(CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_LINE_STRIP, 0, 361);
	CHECK_GL();
	Graphics::Restore();
}
void GLRenderer::StrokeRectangle(float x, float y, float w, float h) {
	StrokeLine(x, y, x + w, y);
	StrokeLine(x, y + h, x + w, y + h);

	StrokeLine(x, y, x, y + h);
	StrokeLine(x + w, y, x + w, y + h);
}
void GLRenderer::FillCircle(float x, float y, float rad) {
#ifdef GL_SUPPORTS_SMOOTHING
	if (Graphics::SmoothFill) {
		glEnable(GL_POLYGON_SMOOTH);
	}
#endif

	Graphics::Save();
	Graphics::Translate(x, y, 0.0f);
	Graphics::Scale(rad, rad, 1.0f);
	GL_Predraw(NULL);

	glBindBuffer(GL_ARRAY_BUFFER, BufferCircleFill);
	glVertexAttribPointer(CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 362);
	CHECK_GL();
	Graphics::Restore();

#ifdef GL_SUPPORTS_SMOOTHING
	if (Graphics::SmoothFill) {
		glDisable(GL_POLYGON_SMOOTH);
	}
#endif
}
void GLRenderer::FillEllipse(float x, float y, float w, float h) {
#ifdef GL_SUPPORTS_SMOOTHING
	if (Graphics::SmoothFill) {
		glEnable(GL_POLYGON_SMOOTH);
	}
#endif

	Graphics::Save();
	Graphics::Translate(x + w / 2, y + h / 2, 0.0f);
	Graphics::Scale(w / 2, h / 2, 1.0f);
	GL_Predraw(NULL);

	glBindBuffer(GL_ARRAY_BUFFER, BufferCircleFill);
	glVertexAttribPointer(CurrentShader->LocPosition, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 362);
	CHECK_GL();
	Graphics::Restore();

#ifdef GL_SUPPORTS_SMOOTHING
	if (Graphics::SmoothFill) {
		glDisable(GL_POLYGON_SMOOTH);
	}
#endif
}
void GLRenderer::FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {
#ifdef GL_SUPPORTS_SMOOTHING
	if (Graphics::SmoothFill) {
		glEnable(GL_POLYGON_SMOOTH);
	}
#endif

	GL_Vec2 v[3];
	v[0] = GL_Vec2{x1, y1};
	v[1] = GL_Vec2{x2, y2};
	v[2] = GL_Vec2{x3, y3};

	GL_Predraw(NULL);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glVertexAttribPointer(CurrentShader->LocPosition, 2, GL_FLOAT, GL_FALSE, 0, v);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	CHECK_GL();

#ifdef GL_SUPPORTS_SMOOTHING
	if (Graphics::SmoothFill) {
		glDisable(GL_POLYGON_SMOOTH);
	}
#endif
}
void GLRenderer::FillRectangle(float x, float y, float w, float h) {
#ifdef GL_SUPPORTS_SMOOTHING
	if (Graphics::SmoothFill) {
		glEnable(GL_POLYGON_SMOOTH);
	}
#endif

	GL_Predraw(NULL);

	GL_Vec2 v[4];
	v[0] = GL_Vec2{x, y};
	v[1] = GL_Vec2{x + w, y};
	v[2] = GL_Vec2{x, y + h};
	v[3] = GL_Vec2{x + w, y + h};
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glVertexAttribPointer(GLRenderer::CurrentShader->LocPosition, 2, GL_FLOAT, GL_FALSE, 0, v);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	CHECK_GL();

#ifdef GL_SUPPORTS_SMOOTHING
	if (Graphics::SmoothFill) {
		glDisable(GL_POLYGON_SMOOTH);
	}
#endif
}
Uint32 GLRenderer::CreateTexturedShapeBuffer(float* data, int vertexCount) {
	// x, y, z, u, v
	Uint32 bufferID;
	glGenBuffers(1, &bufferID);
	glBindBuffer(GL_ARRAY_BUFFER, bufferID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertexCount * 5, data, GL_STATIC_DRAW);
	CHECK_GL();
	return bufferID;
}
void GLRenderer::DrawTexturedShapeBuffer(Texture* texture, Uint32 bufferID, int vertexCount) {
	GL_Predraw(texture);

	// glEnableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord);

	glBindBuffer(GL_ARRAY_BUFFER, bufferID);
	glVertexAttribPointer(GLRenderer::CurrentShader->LocPosition,
		3,
		GL_FLOAT,
		GL_FALSE,
		sizeof(float) * 5,
		(GLvoid*)0);
	glVertexAttribPointer(GLRenderer::CurrentShader->LocTexCoord,
		2,
		GL_FLOAT,
		GL_FALSE,
		sizeof(float) * 5,
		(GLvoid*)12);
	glDrawArrays(GL_TRIANGLES, 0, vertexCount);
	CHECK_GL();

	// glDisableVertexAttribArray(GLRenderer::CurrentShader->LocTexCoord);
}

// Texture drawing functions
void GLRenderer::DrawTexture(Texture* texture,
	float sx,
	float sy,
	float sw,
	float sh,
	float x,
	float y,
	float w,
	float h) {
	x *= RetinaScale;
	y *= RetinaScale;
	w *= RetinaScale;
	h *= RetinaScale;
	GL_DrawTexture(texture, sx, sy, sw, sh, x, y, w, h);
}
void GLRenderer::DrawSprite(ISprite* sprite,
	int animation,
	int frame,
	int x,
	int y,
	bool flipX,
	bool flipY,
	float scaleW,
	float scaleH,
	float rotation,
	unsigned paletteID) {
	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) {
		return;
	}

	// /*
	AnimFrame animframe = sprite->Animations[animation].Frames[frame];
	Graphics::Save();
	// Graphics::Rotate(0.0f, 0.0f, rotation);
	Graphics::Translate(x, y, 0.0f);
	GL_DrawTextureBuffered(sprite->Spritesheets[animframe.SheetNumber],
		sprite->ID,
		animframe.BufferOffset,
		((int)flipY << 1) | (int)flipX);
	Graphics::Restore();
	//*/

	// AnimFrame animframe =
	// sprite->Animations[animation].Frames[frame]; float fX =
	// flipX ? -1.0 : 1.0; float fY = flipY ? -1.0 : 1.0; float sw
	// = animframe.Width; float sh  = animframe.Height;
	//
	// GLRenderer::DrawTexture(sprite->Spritesheets[animframe.SheetNumber],
	//     animframe.X, animframe.Y, sw, sh,
	//     x + fX * animframe.OffsetX,
	//     y + fY * animframe.OffsetY, fX * sw, fY * sh);
}
void GLRenderer::DrawSpritePart(ISprite* sprite,
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
	unsigned paletteID) {
	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) {
		return;
	}

	AnimFrame animframe = sprite->Animations[animation].Frames[frame];
	if (sx == animframe.Width) {
		return;
	}
	if (sy == animframe.Height) {
		return;
	}

	float fX = flipX ? -1.0 : 1.0;
	float fY = flipY ? -1.0 : 1.0;
	if (sw >= animframe.Width - sx) {
		sw = animframe.Width - sx;
	}
	if (sh >= animframe.Height - sy) {
		sh = animframe.Height - sy;
	}

	GLRenderer::DrawTexture(sprite->Spritesheets[animframe.SheetNumber],
		animframe.X + sx,
		animframe.Y + sy,
		sw,
		sh,
		x + fX * (sx + animframe.OffsetX),
		y + fY * (sy + animframe.OffsetY),
		fX * sw,
		fY * sh);
}
// 3D drawing functions
void GLRenderer::DrawPolygon3D(void* data,
	int vertexCount,
	int vertexFlag,
	Texture* texture,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	PolygonRenderer* renderer = GL_GetPolygonRenderer();
	if (renderer != nullptr) {
		renderer->ModelMatrix = modelMatrix;
		renderer->NormalMatrix = normalMatrix;
		renderer->DrawPolygon3D((VertexAttribute*)data, vertexCount, vertexFlag, texture);
	}
}
void GLRenderer::DrawSceneLayer3D(void* layer,
	int sx,
	int sy,
	int sw,
	int sh,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	PolygonRenderer* renderer = GL_GetPolygonRenderer();
	if (renderer != nullptr) {
		renderer->ModelMatrix = modelMatrix;
		renderer->NormalMatrix = normalMatrix;
		renderer->DrawSceneLayer3D((SceneLayer*)layer, sx, sy, sw, sh);
	}
}
void GLRenderer::DrawModel(void* inModel,
	Uint16 animation,
	Uint32 frame,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	PolygonRenderer* renderer = GL_GetPolygonRenderer();
	if (renderer != nullptr) {
		renderer->ModelMatrix = modelMatrix;
		renderer->NormalMatrix = normalMatrix;
		renderer->DrawModel((IModel*)inModel, animation, frame);
	}
}
void GLRenderer::DrawModelSkinned(void* inModel,
	Uint16 armature,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	PolygonRenderer* renderer = GL_GetPolygonRenderer();
	if (renderer != nullptr) {
		renderer->ModelMatrix = modelMatrix;
		renderer->NormalMatrix = normalMatrix;
		renderer->DrawModelSkinned((IModel*)inModel, armature);
	}
}
void GLRenderer::DrawVertexBuffer(Uint32 vertexBufferIndex,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	if (Graphics::CurrentScene3D < 0 || vertexBufferIndex < 0 ||
		vertexBufferIndex >= MAX_VERTEX_BUFFERS) {
		return;
	}

	Scene3D* scene = &Graphics::Scene3Ds[Graphics::CurrentScene3D];
	if (!scene->Initialized) {
		return;
	}

	VertexBuffer* vertexBuffer = Graphics::VertexBuffers[vertexBufferIndex];
	if (!vertexBuffer || !vertexBuffer->FaceCount || !vertexBuffer->VertexCount) {
		return;
	}

	polyRenderer.ScenePtr = scene;
	polyRenderer.VertexBuf = vertexBuffer;
	polyRenderer.DoProjection = false;
	polyRenderer.DoClipping = false;
	polyRenderer.ModelMatrix = modelMatrix;
	polyRenderer.NormalMatrix = normalMatrix;
	polyRenderer.DrawMode = scene->DrawMode;
	polyRenderer.FaceCullMode = scene->FaceCullMode;
	polyRenderer.CurrentColor = ColorUtils::ToRGB(Graphics::BlendColors);
	polyRenderer.DrawVertexBuffer();

	GL_VertexBuffer* driverData = (GL_VertexBuffer*)vertexBuffer->DriverData;
	driverData->Changed = true;

	driverData = (GL_VertexBuffer*)scene->Buffer->DriverData;
	driverData->Changed = true;
}
void GLRenderer::BindVertexBuffer(Uint32 vertexBufferIndex) {}
void GLRenderer::UnbindVertexBuffer() {}
void GLRenderer::ClearScene3D(Uint32 sceneIndex) {
	Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];
	GL_VertexBuffer* driverData = (GL_VertexBuffer*)scene->Buffer->DriverData;
	driverData->Changed = true;
}
void GLRenderer::DrawScene3D(Uint32 sceneIndex, Uint32 drawMode) {
	if (sceneIndex < 0 || sceneIndex >= MAX_3D_SCENES) {
		return;
	}

	Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];
	if (!scene->Initialized) {
		return;
	}

	View* currentView = Graphics::CurrentView;
	if (!currentView) {
		return;
	}

	VertexBuffer* vertexBuffer = scene->Buffer;
	GL_VertexBuffer* driverData = (GL_VertexBuffer*)vertexBuffer->DriverData;

	bool useBatching = true;
	bool remakeVtxIdxBuf = false;
	bool driverDataChanged = driverData->Changed;
	if (driverDataChanged) {
		GL_UpdateVertexBuffer(scene, vertexBuffer, drawMode, useBatching);
		driverData->Changed = false;
		remakeVtxIdxBuf = true;
	}

	size_t numFaces = driverData->Faces->size();
	if (numFaces == 0) {
		return;
	}

	GL_Predraw(NULL);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

#ifdef GL_SUPPORTS_SMOOTHING
	if (Graphics::SmoothFill) {
		glEnable(GL_POLYGON_SMOOTH);
	}
#endif

	glPointSize(scene->PointSize);

	Matrix4x4 projMat = scene->ProjectionMatrix;
	Matrix4x4 viewMat = scene->ViewMatrix;

	Matrix4x4* out = Graphics::ModelViewMatrix;
	float cx = (float)(out->Values[12] - currentView->X) / currentView->Width;
	float cy = (float)(out->Values[13] - currentView->Y) / currentView->Height;

	Matrix4x4 identity;
	Matrix4x4::Identity(&identity);
	Matrix4x4::Translate(&identity, &identity, cx, cy, 0.0f);
	if (currentView->UseDrawTarget) {
		Matrix4x4::Scale(&identity, &identity, 1.0f, -1.0f, 1.0f);
	}
	Matrix4x4::Multiply(&projMat, &identity, &projMat);

	// should transpose this
	Matrix4x4::Transpose(&viewMat);

	// Prepare the shader now
	GLRenderer::UseShader(GLRenderer::ShaderShape3D->Get(true));
	GL_SetProjectionMatrix(&projMat);
	GL_SetModelViewMatrix(&viewMat);

	// Begin drawing
	GL_State state = {0};

	GLenum cullWindingOrder = currentView->UseDrawTarget ? GL_CCW : GL_CW;

#ifdef HAVE_GL_PERFSTATS
	GL_PerfStats perf = {0};
	PERF_START(perf);
#endif

	GLRenderer::SetDepthTesting(true);

	// Draw it all in one go if we can
	if (useBatching && driverData->UseVertexIndices) {
		GL_UpdateStateFromFace(state, (*driverData->Faces)[0], scene, cullWindingOrder);
		GL_SetState(state, driverData, &projMat, &viewMat);
		PERF_STATE_CHANGE(perf);

		size_t numBatches = driverData->VertexIndexList.size();
		for (size_t i = 0; i < numBatches; i++) {
			GL_SetVertexAttribPointers((*driverData->Entries)[i], false);
			GL_DrawBatchedScene3D(driverData,
				driverData->VertexIndexList[i],
				state.PrimitiveType,
				remakeVtxIdxBuf);
			PERF_DRAW_CALL(perf);
		}
	}
	else {
		GL_State lastStateCMP = {0};
		lastStateCMP.VertexAtrribs = NULL;
		GL_State lastState = {0};
		GL_Scene3DBatch batch = {0};

		batch.VertexIndices.clear();

		for (size_t f = 0; f < numFaces; f++) {
			bool stateChanged = false;

			GL_VertexBufferFace& face = (*driverData->Faces)[f];
			GL_UpdateStateFromFace(state, face, scene, cullWindingOrder);

			// Force a state change if this is the first
			// face
			if (f == 0) {
				memcpy(&lastState, &state, sizeof(GL_State));
				GL_SetState(state, driverData, &projMat, &viewMat);
				PERF_STATE_CHANGE(perf);
			}
			else if (memcmp(&lastState, &state, sizeof(GL_State))) {
				stateChanged = true;
			}

			bool didDraw = false;

			if (useBatching) {
				if (stateChanged) {
					// the most common state change
					// is ONLY a texture change.
					// check it and cull mode, save
					// a round trip
					if (GL_ActiveCullMode != lastState.CullMode) {
						glCullFace(GL_ActiveCullMode = lastState.CullMode);
					}

					lastStateCMP.CullMode = lastState.CullMode;
					lastStateCMP.TexturePtr = lastState.TexturePtr;
					if (memcmp(&lastState, &lastStateCMP, sizeof(GL_State))) {
						GL_SetState(lastState,
							driverData,
							&projMat,
							&viewMat,
							lastStateCMP.VertexAtrribs == NULL
								? NULL
								: &lastStateCMP);
						PERF_STATE_CHANGE(perf);
					}
					else { // literally just a
						// texture change,
						// rebind it
						GL_BindTexture(lastState.TexturePtr, GL_REPEAT);
					}

					// Draw the current batch, then
					// start the next
					if (batch.ShouldDraw) {
						GL_DrawBatchedScene3D(driverData,
							&batch.VertexIndices,
							lastState.PrimitiveType,
							true);
						PERF_DRAW_CALL(perf);
						batch.VertexIndices.clear();
						didDraw = true;
					}
				}

				// Push this face's vertex indices
				for (unsigned i = 0; i < 3; i++) {
					batch.VertexIndices.push_back(face.VertexIndices[i]);
				}

				batch.ShouldDraw = true;
			}
			else {
				// Just draw the face right away if not
				// batching
				if (stateChanged) {
					GL_SetState(lastState,
						driverData,
						&projMat,
						&viewMat,
						lastStateCMP.VertexAtrribs == NULL ? NULL
										   : &lastStateCMP);
					PERF_STATE_CHANGE(perf);
				}
				didDraw = true;

				glDrawArrays(
					state.PrimitiveType, face.VertexIndex, face.NumVertices);
				CHECK_GL();
				PERF_DRAW_CALL(perf);
			}

			// Remember the current state
			if (stateChanged) {
				memcpy(&lastStateCMP, &lastState, sizeof(GL_State));
				memcpy(&lastState, &state, sizeof(GL_State));
			}

			// If this is the last face and the batch was
			// already drawn, then nothing more needs to be
			if (f == numFaces - 1 && didDraw) {
				batch.ShouldDraw = false;
			}
		}

		// Draw the last remaining batch
		if (batch.ShouldDraw) {
			if (GL_ActiveCullMode != lastState.CullMode) {
				glCullFace(GL_ActiveCullMode = lastState.CullMode);
			}

			lastStateCMP.CullMode = lastState.CullMode;
			lastStateCMP.TexturePtr = lastState.TexturePtr;
			if (memcmp(&lastState, &lastStateCMP, sizeof(GL_State))) {
				GL_SetState(lastState,
					driverData,
					&projMat,
					&viewMat,
					lastStateCMP.VertexAtrribs == NULL ? NULL : &lastStateCMP);
				PERF_STATE_CHANGE(perf);
			}
			else { // literally just a texture change,
				// rebind it
				GL_BindTexture(lastState.TexturePtr, GL_REPEAT);
			}

			GL_DrawBatchedScene3D(
				driverData, &batch.VertexIndices, state.PrimitiveType, true);
			PERF_DRAW_CALL(perf);
		}
	}

#ifdef HAVE_GL_PERFSTATS
	PERF_END(perf);

	Log::Print(Log::LOG_VERBOSE, "Frame %d:", Graphics::CurrentFrame);
	Log::Print(Log::LOG_VERBOSE, "  Scene 3D %d:", sceneIndex);
	Log::Print(Log::LOG_VERBOSE, "  - Draw calls: %d", perf.DrawCalls);
	Log::Print(Log::LOG_VERBOSE, "  - State changes: %d", perf.StateChanges);
	Log::Print(Log::LOG_VERBOSE,
		"  - Updated vertex buffer: %s",
		driverDataChanged ? "Yes" : "No");
	Log::Print(Log::LOG_VERBOSE, "  - Time taken: %3.3f ms", perf.Time);
#endif

	GL_Predraw(NULL);

#ifdef GL_SUPPORTS_SMOOTHING
	if (Graphics::SmoothFill) {
		glDisable(GL_POLYGON_SMOOTH);
	}
#endif

	glPointSize(1.0f);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);
	glFrontFace(GL_CCW);

	GLRenderer::SetDepthTesting(Graphics::UseDepthTesting);
}

void* GLRenderer::CreateVertexBuffer(Uint32 maxVertices) {
	VertexBuffer* vtxBuf = new VertexBuffer(maxVertices);
	vtxBuf->DriverData =
		Memory::TrackedCalloc("VertexBuffer::DriverData", 1, sizeof(GL_VertexBuffer));

	GL_VertexBuffer* driverData = (GL_VertexBuffer*)vtxBuf->DriverData;

	GL_ReallocVertexBuffer(driverData, maxVertices);

	driverData->Changed = true;
	driverData->VertexIndexList.clear();
	driverData->UseVertexIndices = false;

	return (void*)vtxBuf;
}
void GLRenderer::DeleteVertexBuffer(void* vtxBuf) {
	VertexBuffer* vertexBuffer = (VertexBuffer*)vtxBuf;
	GL_VertexBuffer* driverData = (GL_VertexBuffer*)vertexBuffer->DriverData;
	if (driverData) {
		GL_DeleteVertexIndexList(driverData);

		for (size_t e = 0; e < driverData->Entries->size(); e++) {
			Memory::Free((*driverData->Entries)[e]);
		}

		if (driverData->VertexIndexBuffer) {
			Memory::Free(driverData->VertexIndexBuffer);
		}

		delete driverData->Entries;
		delete driverData->Faces;

		Memory::Free(driverData);
	}

	delete vertexBuffer;
}
void GLRenderer::MakeFrameBufferID(ISprite* sprite) {
	float fX[4], fY[4];
	fX[0] = 1.0;
	fY[0] = 1.0;
	fX[1] = -1.0;
	fY[1] = 1.0;
	fX[2] = 1.0;
	fY[2] = -1.0;
	fX[3] = -1.0;
	fY[3] = -1.0;

	GL_AnimFrameVert vertices[16];

	if (sprite->ID == 0) {
		glGenBuffers(1, (GLuint*)&sprite->ID);
	}
	glBindBuffer(GL_ARRAY_BUFFER, sprite->ID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) * sprite->FrameCount, NULL, GL_STATIC_DRAW);
	CHECK_GL();

	size_t fc = 0;
	for (size_t a = 0; a < sprite->Animations.size(); ++a) {
		for (size_t i = 0; i < sprite->Animations[a].Frames.size(); ++i) {
			AnimFrame* frame = &sprite->Animations[a].Frames[i];
			GL_AnimFrameVert* vert = &vertices[0];

			if (frame->SheetNumber >= sprite->Spritesheets.size()) {
				continue;
			}

			float texWidth = sprite->Spritesheets[frame->SheetNumber]->Width;
			float texHeight = sprite->Spritesheets[frame->SheetNumber]->Height;

			float ffU0 = frame->X / texWidth;
			float ffV0 = frame->Y / texHeight;
			float ffU1 = (frame->X + frame->Width) / texWidth;
			float ffV1 = (frame->Y + frame->Height) / texHeight;

			float _fX, _fY, ffX0, ffY0, ffX1, ffY1;
			for (int f = 0; f < 4; f++) {
				_fX = fX[f];
				_fY = fY[f];
				ffX0 = _fX * frame->OffsetX;
				ffY0 = _fY * frame->OffsetY;
				ffX1 = _fX * (frame->OffsetX + frame->Width);
				ffY1 = _fY * (frame->OffsetY + frame->Height);
				vert[0] = GL_AnimFrameVert{ffX0, ffY0, ffU0, ffV0};
				vert[1] = GL_AnimFrameVert{ffX1, ffY0, ffU1, ffV0};
				vert[2] = GL_AnimFrameVert{ffX0, ffY1, ffU0, ffV1};
				vert[3] = GL_AnimFrameVert{ffX1, ffY1, ffU1, ffV1};
				vert += 4;
			}

			frame->BufferOffset = fc++ * sizeof(vertices);
			glBufferSubData(
				GL_ARRAY_BUFFER, frame->BufferOffset, sizeof(vertices), vertices);
		}
	}

	CHECK_GL();
}
void GLRenderer::DeleteFrameBufferID(ISprite* sprite) {
	if (sprite->ID) {
		glDeleteBuffers(1, (GLuint*)&sprite->ID);
		sprite->ID = 0;
	}
}
void GLRenderer::SetDepthTesting(bool enable) {
	if (UseDepthTesting) {
		if (enable) {
			glEnable(GL_DEPTH_TEST);
		}
		else {
			glDisable(GL_DEPTH_TEST);
		}
	}
}

#endif /* USING_OPENGL */
