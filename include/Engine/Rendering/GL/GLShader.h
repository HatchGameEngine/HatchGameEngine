#ifndef ENGINE_RENDERING_GL_GLSHADER_H
#define ENGINE_RENDERING_GL_GLSHADER_H

#include <Engine/Rendering/Shader.h>
#include <Engine/Rendering/GL/Includes.h>

typedef std::unordered_map<std::string, GLint> GLVariableMap;

class GLShader : public Shader {
private:
	void AddVertexProgram(Stream* stream);
	void AddFragmentProgram(Stream* stream);
	bool AttachAndLink();
	std::string CheckShaderError(GLuint shader);
	std::string CheckProgramError(GLuint prog);

	GLVariableMap AttribMap;
	GLVariableMap UniformMap;

public:
	GLuint ProgramID = 0;
	GLuint VertexProgramID = 0;
	GLuint FragmentProgramID = 0;
	GLint LocProjectionMatrix;
	GLint LocViewMatrix;
	GLint LocModelMatrix;
	GLint LocPosition;
	GLint LocTexCoord;
	GLint LocTexture;
	GLint LocTextureSize;
	GLint LocSpriteFrameCoords;
	GLint LocSpriteFrameSize;
#ifdef GL_HAVE_YUV
	GLint LocTextureU;
	GLint LocTextureV;
#endif
	GLint LocPaletteTexture;
	GLint LocPaletteLine;
	GLint LocPaletteIndexTable;
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

	GLShader();
	GLShader(std::string vertexShaderSource, std::string fragmentShaderSource);
	GLShader(Stream* streamVS, Stream* streamFS);

	GLint GetAttribLocation(std::string identifier);
	GLint GetUniformLocation(std::string identifier);

	void Compile();
	void AddProgram(int program, Stream* stream);
	bool HasProgram(int program);
	bool IsValid();

	bool HasUniform(const char* name);
	void SetUniform(const char* name, size_t count, int* values);
	void SetUniform(const char* name, size_t count, float* values);
	void SetUniformArray(const char* name, size_t count, int* values, size_t numValues);
	void SetUniformArray(const char* name, size_t count, float* values, size_t numValues);
	void SetUniformTexture(const char* name, Texture* texture, int slot);

	void Use();
	void Validate();
	void Delete();

	virtual ~GLShader();
};

#endif /* ENGINE_RENDERING_GL_GLSHADER_H */
