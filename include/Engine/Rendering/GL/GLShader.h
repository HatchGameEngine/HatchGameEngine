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
#define UNIFORM_PALETTEINDEXTEXTURE "u_paletteIndexTexture"
#ifdef GL_HAVE_YUV
#define UNIFORM_TEXTUREU "u_textureU"
#define UNIFORM_TEXTUREV "u_textureV"
#endif

struct GL_ProcessedShader {
	char* SourceText;
	std::vector<std::string> Defines;
};

#define SHADER_FEATURE_TEXTURE (1 << 0)
#define SHADER_FEATURE_PALETTE (1 << 1)
#define SHADER_FEATURE_MATERIALS (1 << 2)
#define SHADER_FEATURE_VERTEXCOLORS (1 << 3)
#define SHADER_FEATURE_FOG_LINEAR (1 << 4)
#define SHADER_FEATURE_FOG_EXP (1 << 5)

#define SHADER_FEATURE_FOG_FLAGS (SHADER_FEATURE_FOG_LINEAR | SHADER_FEATURE_FOG_EXP)
#define SHADER_FEATURE_ALL ((1 << 6) - 1)

#define NUM_SHADER_FEATURES (SHADER_FEATURE_ALL + 1)

class GLShader : public Shader {
private:
	static char* FindInclude(std::string identifier);
	static GL_ProcessedShader ProcessFragmentShaderText(char* text);
	static std::vector<char*> GetShaderSources(GL_ProcessedShader processed);

	void AddVertexShader(Stream* stream);
	void AddFragmentShader(Stream* stream);
	void AttachAndLink();

	std::string CheckShaderError(GLuint shader);
	std::string CheckProgramError(GLuint prog);

	static Uint8 ConvertDataTypeToEnum(GLenum type);

	static void SendUniformValues(int location, size_t count, void* values, Uint8 type);

#ifndef GL_USING_ATTRIB_LOCATIONS
	GLVariableMap AttribMap;
#endif

public:
	static void InitIncludes();

	GLShader();
	GLShader(std::string vertexShaderSource, std::string fragmentShaderSource);
	GLShader(Stream* streamVS, Stream* streamFS);

	void Compile();
	void AddStage(int stage, Stream* stream);
	bool HasStage(int stage);
	bool HasRequiredStages();
	bool CanCompile();
	bool IsValid();

	bool HasUniform(const char* name);
	void SetUniform(ShaderUniform* uniform, size_t count, void* values, Uint8 type);
	void SetUniformTexture(const char* name, Texture* texture);
	void SetUniformTexture(int uniform, Texture* texture);
	void SetUniformTexture(int uniform, int textureID);

	void Use();
	void Validate();
	void ValidateStages();

	int GetAttribLocation(std::string identifier);
	int GetRequiredAttrib(std::string identifier);
	int GetUniformLocation(std::string identifier);

	void InitUniforms();
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
	GLint LocPaletteIndexTexture;
	GLint LocPaletteID;
	GLint LocNumTexturePaletteIndices;
	GLint LocColor;
	GLint LocDiffuseColor;
	GLint LocSpecularColor;
	GLint LocAmbientColor;
	GLint LocVaryingColor;
	GLint LocFogColor;
	GLint LocFogLinearStart;
	GLint LocFogLinearEnd;
	GLint LocFogDensity;
	GLint LocFogSmoothness;

#if GL_USING_ATTRIB_LOCATIONS
	std::unordered_map<std::string, int> AttribLocationMap;
#endif
};

#endif /* ENGINE_RENDERING_GL_GLSHADER_H */
