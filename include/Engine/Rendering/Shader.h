#ifndef ENGINE_RENDERING_SHADER_H
#define ENGINE_RENDERING_SHADER_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/Texture.h>

struct ShaderUniform {
	std::string Name;
	Uint8 Type;
	bool IsArray;
	int Location;
};

class Shader {
protected:
	bool Compiled = false;

	void ValidateTextureUniformNames();

public:
	enum { PROGRAM_VERTEX, PROGRAM_FRAGMENT };

	enum {
		UNIFORM_UNKNOWN,
		UNIFORM_FLOAT,
		UNIFORM_FLOAT_VEC2,
		UNIFORM_FLOAT_VEC3,
		UNIFORM_FLOAT_VEC4,
		UNIFORM_INT,
		UNIFORM_INT_VEC2,
		UNIFORM_INT_VEC3,
		UNIFORM_INT_VEC4,
		UNIFORM_BOOL,
		UNIFORM_BOOL_VEC2,
		UNIFORM_BOOL_VEC3,
		UNIFORM_BOOL_VEC4,
		UNIFORM_FLOAT_MAT2,
		UNIFORM_FLOAT_MAT3,
		UNIFORM_FLOAT_MAT4,
		UNIFORM_SAMPLER_2D,
		UNIFORM_SAMPLER_CUBE
	};

	virtual void Compile();
	virtual void AddProgram(int program, Stream* stream);
	virtual bool HasProgram(int program);
	virtual bool CanCompile();
	virtual bool IsValid();
	bool WasCompiled();

	virtual bool HasUniform(const char* name);
	virtual void SetUniform(ShaderUniform* uniform, size_t count, void* values, Uint8 type);
	virtual void SetUniformTexture(const char* name, Texture* texture);

	virtual void Use();
	virtual void Validate();

	ShaderUniform* GetUniform(std::string identifier);

	virtual int GetAttribLocation(std::string identifier);
	virtual int GetUniformLocation(std::string identifier);

	int AddBuiltinUniform(std::string identifier);
	bool IsBuiltinUniform(std::string identifier);

	virtual void InitUniforms();
	virtual void InitTextureUniforms();

	void AddTextureUniformName(std::string identifier);
	void InitTextureUnitMap();
	int GetTextureUnit(int uniform);

	virtual void Delete();
	virtual ~Shader();

	static size_t GetUniformTypeElementCount(Uint8 type);
	static size_t GetMatrixUniformTypeSize(Uint8 type);
	static bool UniformTypeIsMatrix(Uint8 type);

	std::unordered_map<std::string, ShaderUniform> UniformMap;
	std::vector<std::string> BuiltinUniforms;

	std::vector<std::string> TextureUniformNames;
	std::unordered_map<int, int> TextureUnitMap;

	void* Object = nullptr;

	// Cache stuff
	float CachedBlendColors[4];
	Matrix4x4* CachedProjectionMatrix = nullptr;
	Matrix4x4* CachedViewMatrix = nullptr;
	Matrix4x4* CachedModelMatrix = nullptr;
};

#endif /* ENGINE_RENDERING_SHADER_H */
