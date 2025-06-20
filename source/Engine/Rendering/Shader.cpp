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

bool Shader::IsValid() {
	return false;
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
void Shader::SetUniformTexture(const char* name, Texture* texture, int slot) {
	throw std::runtime_error("Shader::SetUniformTexture() called without an implementation");
}

void Shader::Use() {
	throw std::runtime_error("Shader::Use() called without an implementation");
}
void Shader::Validate() {
	throw std::runtime_error("Shader::Validate() called without an implementation");
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
