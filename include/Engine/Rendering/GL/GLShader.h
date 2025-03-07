#ifndef ENGINE_RENDERING_GL_GLSHADER_H
#define ENGINE_RENDERING_GL_GLSHADER_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/GL/Includes.h>

#ifdef DEBUG
#define GL_DO_ERROR_CHECKING
#endif

#ifdef GL_DO_ERROR_CHECKING
#define CHECK_GL() GLShader::CheckGLError(__LINE__)
#else
#define CHECK_GL()
#endif

class GLShader {
private:
	void AttachAndLink();

public:
	GLuint ProgramID = 0;
	GLuint VertexProgramID = 0;
	GLuint FragmentProgramID = 0;
	GLint LocProjectionMatrix;
	GLint LocModelViewMatrix;
	GLint LocPosition;
	GLint LocTexCoord;
	GLint LocTexture;
	GLint LocTextureU;
	GLint LocTextureV;
	GLint LocPalette;
	GLint LocColor;
	GLint LocDiffuseColor;
	GLint LocSpecularColor;
	GLint LocAmbientColor;
	GLint LocVaryingColor;
	GLint LocFogColor;
	GLint LocFogLinearStart;
	GLint LocFogLinearEnd;
	GLint LocFogDensity;
	GLint LocFogTable;
	char FilenameV[256];
	char FilenameF[256];
	// Cache stuff
	float CachedBlendColors[4];
	Matrix4x4* CachedProjectionMatrix = NULL;
	Matrix4x4* CachedModelViewMatrix = NULL;

	GLShader(std::string vertexShaderSource, std::string fragmentShaderSource);
	GLShader(Stream* streamVS, Stream* streamFS);
	bool CheckShaderError(GLuint shader);
	bool CheckProgramError(GLuint prog);
	GLuint Use();
	GLint GetAttribLocation(const GLchar* identifier);
	GLint GetUniformLocation(const GLchar* identifier);
	~GLShader();
	static bool CheckGLError(int line);
};

#endif /* ENGINE_RENDERING_GL_GLSHADER_H */
