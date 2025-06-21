#ifndef ENGINE_RENDERING_SHADER_H
#define ENGINE_RENDERING_SHADER_H

#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>
#include <Engine/Math/Matrix4x4.h>
#include <Engine/Rendering/Texture.h>

class Shader {
public:
	enum {
		PROGRAM_VERTEX,
		PROGRAM_FRAGMENT
	};

	virtual void Compile();
	virtual void AddProgram(int program, Stream* stream);
	virtual bool HasProgram(int program);
	virtual bool IsValid();

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

	virtual void InitTextureUniformUnits();
	virtual void SetTextureUniformUnit(std::string identifier, int unit);
	void InitTextureUnitMap();
	int GetTextureUnit(int uniform);

	virtual void Delete();
	virtual ~Shader();

	std::vector<std::string> BuiltinUniforms;

	std::unordered_map<std::string, int> TextureUniformMap;
	std::unordered_map<int, int> TextureUnitMap;

	// Cache stuff
	float CachedBlendColors[4];
	Matrix4x4* CachedProjectionMatrix = NULL;
	Matrix4x4* CachedViewMatrix = NULL;
	Matrix4x4* CachedModelMatrix = NULL;
};

#endif /* ENGINE_RENDERING_SHADER_H */
