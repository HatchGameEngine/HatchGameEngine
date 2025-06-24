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
		DATATYPE_UNKNOWN = -1,
		DATATYPE_FLOAT,
		DATATYPE_FLOAT_VEC2,
		DATATYPE_FLOAT_VEC3,
		DATATYPE_FLOAT_VEC4,
		DATATYPE_INT,
		DATATYPE_INT_VEC2,
		DATATYPE_INT_VEC3,
		DATATYPE_INT_VEC4,
		DATATYPE_BOOL,
		DATATYPE_BOOL_VEC2,
		DATATYPE_BOOL_VEC3,
		DATATYPE_BOOL_VEC4,
		DATATYPE_FLOAT_MAT2,
		DATATYPE_FLOAT_MAT3,
		DATATYPE_FLOAT_MAT4,
		DATATYPE_SAMPLER_2D,
		DATATYPE_SAMPLER_CUBE
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

	static size_t GetDataTypeElementCount(Uint8 type);
	static size_t GetMatrixDataTypeSize(Uint8 type);
	static bool DataTypeIsMatrix(Uint8 type);
	static bool DataTypeIsSampler(Uint8 type);

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
