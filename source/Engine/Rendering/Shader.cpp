#include <Engine/Diagnostics/Log.h>
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
void Shader::SetUniform(ShaderUniform* uniform, size_t count, void* values, Uint8 type) {
	throw std::runtime_error("Shader::SetUniform() called without an implementation");
}
void Shader::SetUniformTexture(const char* name, Texture* texture) {
	throw std::runtime_error("Shader::SetUniformTexture() called without an implementation");
}

size_t Shader::GetDataTypeElementCount(Uint8 type) {
	switch (type) {
	case DATATYPE_INT_VEC2:
	case DATATYPE_FLOAT_VEC2:
	case DATATYPE_BOOL_VEC2:
		return 2;
	case DATATYPE_INT_VEC3:
	case DATATYPE_FLOAT_VEC3:
	case DATATYPE_BOOL_VEC3:
		return 3;
	case DATATYPE_INT_VEC4:
	case DATATYPE_FLOAT_VEC4:
	case DATATYPE_BOOL_VEC4:
		return 4;
	default:
		return 1;
	}
}
size_t Shader::GetMatrixDataTypeSize(Uint8 type) {
	switch (type) {
	case DATATYPE_FLOAT_MAT2:
		return 2 * 4;
	case DATATYPE_FLOAT_MAT3:
		return 3 * 4;
	case DATATYPE_FLOAT_MAT4:
		return 4 * 4;
	default:
		return 0;
	}
}
bool Shader::DataTypeIsMatrix(Uint8 type) {
	switch (type) {
	case DATATYPE_FLOAT_MAT2:
	case DATATYPE_FLOAT_MAT3:
	case DATATYPE_FLOAT_MAT4:
		return true;
	}

	return false;
}
bool Shader::DataTypeIsSampler(Uint8 type) {
	switch (type) {
	case DATATYPE_SAMPLER_2D:
	case DATATYPE_SAMPLER_CUBE:
		return true;
	}

	return false;
}

void Shader::Use() {
	throw std::runtime_error("Shader::Use() called without an implementation");
}
void Shader::Validate() {
	throw std::runtime_error("Shader::Validate() called without an implementation");
}

ShaderUniform* Shader::GetUniform(std::string identifier) {
	std::unordered_map<std::string, ShaderUniform>::iterator it = UniformMap.find(identifier);
	if (it != UniformMap.end()) {
		return &UniformMap[identifier];
	}

	return nullptr;
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

void Shader::InitUniforms() {
	throw std::runtime_error("Shader::InitUniforms() called without an implementation");
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
void Shader::ValidateTextureUniformNames() {
	for (size_t i = 0; i < TextureUniformNames.size();) {
		std::string uniformName = TextureUniformNames[i];

		int uniform = GetUniformLocation(uniformName);
		if (uniform == -1) {
			if (!IsBuiltinUniform(uniformName)) {
				Log::Print(Log::LOG_WARN,
					"No uniform named \"%s\"!",
					uniformName.c_str());
			}

			TextureUniformNames.erase(TextureUniformNames.begin() + i);
			continue;
		}

		i++;
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
				throw std::runtime_error(
					"No uniform named \"" + uniformName + "\"!");
			}

			continue;
		}

		if (unit >= maxTextureUnit) {
			char buffer[64];
			snprintf(buffer,
				sizeof buffer,
				"Too many texture units in use! (Maximum supported is %d)",
				maxTextureUnit);
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
