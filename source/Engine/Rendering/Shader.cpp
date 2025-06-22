#include <Engine/Graphics.h>
#include <Engine/Rendering/Shader.h>

void Shader::Compile() {
	throw std::runtime_error("Shader::Compile() called without an implementation");
}

void Shader::AddProgram(int program, Stream* stream) {
	throw std::runtime_error("Shader::AddProgram() called without an implementation");
}

bool Shader::HasProgram(int program) {
	return false;
}
bool Shader::CanCompile() {
	return false;
}
bool Shader::IsValid() {
	return false;
}
bool Shader::WasCompiled() {
	return Compiled;
}

bool Shader::HasUniform(const char* name) {
	return false;
}
void Shader::SetUniform(const char* name, size_t count, int* values) {
	throw std::runtime_error("Shader::SetUniform() called without an implementation");
}
void Shader::SetUniform(const char* name, size_t count, float* values) {
	throw std::runtime_error("Shader::SetUniform() called without an implementation");
}
void Shader::SetUniformArray(const char* name, size_t count, int* values, size_t numValues) {
	throw std::runtime_error("Shader::SetUniformArray() called without an implementation");
}
void Shader::SetUniformArray(const char* name, size_t count, float* values, size_t numValues) {
	throw std::runtime_error("Shader::SetUniformArray() called without an implementation");
}
void Shader::SetUniformTexture(const char* name, Texture* texture) {
	throw std::runtime_error("Shader::SetUniformTexture() called without an implementation");
}

void Shader::Use() {
	throw std::runtime_error("Shader::Use() called without an implementation");
}
void Shader::Validate() {
	throw std::runtime_error("Shader::Validate() called without an implementation");
}

int Shader::GetAttribLocation(std::string identifier) {
	return -1;
}
int Shader::GetUniformLocation(std::string identifier) {
	return -1;
}

int Shader::AddBuiltinUniform(std::string identifier) {
	int uniform = GetUniformLocation(identifier);

	if (!IsBuiltinUniform(identifier)) {
		BuiltinUniforms.push_back(identifier);
	}

	return uniform;
}
bool Shader::IsBuiltinUniform(std::string identifier) {
	auto it = std::find(BuiltinUniforms.begin(), BuiltinUniforms.end(), identifier);
	return it != BuiltinUniforms.end();
}

void Shader::InitTextureUniforms() {
	throw std::runtime_error("Shader::InitTextureUniforms() called without an implementation");
}
void Shader::AddTextureUniformName(std::string identifier) {
	auto it = std::find(TextureUniformNames.begin(), TextureUniformNames.end(), identifier);
	if (it == TextureUniformNames.end()) {
		TextureUniformNames.push_back(identifier);
	}
}
void Shader::InitTextureUnitMap() {
	int maxTextureUnit = Graphics::GetMaxTextureUnits();
	int unit = 0;

	for (size_t i = 0, iSz = TextureUniformNames.size(); i < iSz; i++) {
		std::string uniformName = TextureUniformNames[i];

		int uniform = GetUniformLocation(uniformName);
		if (uniform == -1) {
			if (!IsBuiltinUniform(uniformName)) {
				throw std::runtime_error("No uniform named \"" + uniformName + "\"!");
			}

			continue;
		}

		if (unit >= maxTextureUnit) {
			char buffer[64];
			snprintf(buffer, sizeof buffer, "Too many texture units in use! (Maximum supported is %d)", maxTextureUnit);
			throw std::runtime_error(std::string(buffer));
		}

		TextureUnitMap[uniform] = unit;

		unit++;
	}

	TextureUniformNames.clear();
}

int Shader::GetTextureUnit(int uniform) {
	if (uniform == -1) {
		return -1;
	}

	std::unordered_map<int, int>::iterator it = TextureUnitMap.find(uniform);
	if (it != TextureUnitMap.end()) {
		return it->second;
	}

	return -1;
}

void Shader::Delete() {
	if (CachedProjectionMatrix) {
		delete CachedProjectionMatrix;
		CachedProjectionMatrix = nullptr;
	}
	if (CachedViewMatrix) {
		delete CachedViewMatrix;
		CachedViewMatrix = nullptr;
	}
	if (CachedModelMatrix) {
		delete CachedModelMatrix;
		CachedModelMatrix = nullptr;
	}
}

Shader::~Shader() {
	Delete();
}
