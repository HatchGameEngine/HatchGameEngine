#ifndef ENGINE_RENDERING_GL_GLSHADER_H
#define ENGINE_RENDERING_GL_GLSHADER_H

#include <Engine/Rendering/Shader.h>
#include <Engine/Rendering/GL/Includes.h>

typedef std::unordered_map<std::string, GLint> GLVariableMap;

#define ATTRIB_POSITION "i_position"
#define ATTRIB_UV "i_uv"
#define ATTRIB_COLOR "i_color"

#define UNIFORM_TEXTURE "u_texture"
#define UNIFORM_PALETTETEXTURE "u_paletteTexture"
#ifdef GL_HAVE_YUV
#define UNIFORM_TEXTUREU "u_textureU"
#define UNIFORM_TEXTUREV "u_textureV"
#endif

class GLShader : public Shader {
private:
	void AddVertexProgram(Stream* stream);
	void AddFragmentProgram(Stream* stream);
	void AttachAndLink();
	std::string CheckShaderError(GLuint shader);
	std::string CheckProgramError(GLuint prog);

	GLVariableMap AttribMap;
	GLVariableMap UniformMap;

public:
	GLShader();
	GLShader(std::string vertexShaderSource, std::string fragmentShaderSource);
	GLShader(Stream* streamVS, Stream* streamFS);

	void Compile();
	void AddProgram(int program, Stream* stream);
	bool HasProgram(int program);
	bool IsValid();

	bool HasUniform(const char* name);
	void SetUniform(const char* name, size_t count, int* values);
	void SetUniform(const char* name, size_t count, float* values);
	void SetUniformArray(const char* name, size_t count, int* values, size_t numValues);
	void SetUniformArray(const char* name, size_t count, float* values, size_t numValues);
	void SetUniformTexture(const char* name, Texture* texture);
	void SetUniformTexture(int uniform, Texture* texture);
	void SetUniformTexture(int uniform, int textureID);

	void Use();
	void Validate();

	int GetAttribLocation(std::string identifier);
	int GetRequiredAttrib(std::string identifier);
	int GetUniformLocation(std::string identifier);

	void InitTextureUniforms();

#if GL_USING_ATTRIB_LOCATIONS
	void InitAttributes();
	void BindAttribLocations();
#endif

	void Delete();
	virtual ~GLShader();

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

#if GL_USING_ATTRIB_LOCATIONS
	std::unordered_map<std::string, int> AttribLocationMap;
#endif
};

#endif /* ENGINE_RENDERING_GL_GLSHADER_H */
