#ifndef ENGINE_RENDERING_SHADER_H
#define ENGINE_RENDERING_SHADER_H

#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/Texture.h>

class Shader {
protected:
	bool Compiled = false;

public:
	enum {
		PROGRAM_VERTEX,
		PROGRAM_FRAGMENT
	};

	virtual void Compile();
	virtual void AddProgram(int program, Stream* stream);
	virtual bool HasProgram(int program);
	virtual bool CanCompile();
	virtual bool IsValid();
	bool WasCompiled();

	virtual bool HasUniform(const char* name);
	virtual void SetUniform(const char* name, size_t count, int* values);
	virtual void SetUniform(const char* name, size_t count, float* values);
	virtual void SetUniformArray(const char* name, size_t count, int* values, size_t numValues);
	virtual void SetUniformArray(const char* name, size_t count, float* values, size_t numValues);
	virtual void SetUniformTexture(const char* name, Texture* texture);

	virtual void Use();
	virtual void Validate();

	virtual int GetAttribLocation(std::string identifier);
	virtual int GetUniformLocation(std::string identifier);

	int AddBuiltinUniform(std::string identifier);
	bool IsBuiltinUniform(std::string identifier);

	virtual void InitTextureUniforms();
	void AddTextureUniformName(std::string identifier);
	void InitTextureUnitMap();
	int GetTextureUnit(int uniform);

	virtual void Delete();
	virtual ~Shader();

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
