#ifdef USING_OPENGL

#include <Engine/IO/TextStream.h>
#include <Engine/Rendering/GL/GLRenderer.h>
#include <Engine/Rendering/GL/GLShader.h>
#include <Engine/Utilities/StringUtils.h>

std::unordered_map<std::string, const char*> shaderIncludes;

void GLShader::InitIncludes() {
	shaderIncludes["TEXTURE_SAMPLING_FUNCTIONS"] = R"(
#define PALETTE_INDEX_TABLE_ID -1
#define MAX_PALETTE_INDEX_LINES 4096

uniform sampler2D u_paletteTexture;
uniform int u_paletteID;
uniform int u_paletteIndexTable[MAX_PALETTE_INDEX_LINES];

int hatch_getPaletteLine(int paletteID) {
    if (paletteID == PALETTE_INDEX_TABLE_ID) {
        int screenLine = clamp(int(gl_FragCoord.y), 0, MAX_PALETTE_INDEX_LINES - 1);
        return u_paletteIndexTable[screenLine];
    }

    return paletteID;
}

vec4 hatch_getColorFromPalette(int paletteIndex, sampler2D paletteTexture, int paletteLine) {
    if (paletteIndex == 0) {
        return vec4(0.0, 0.0, 0.0, 0.0);
    }

    vec2 paletteTextureCoords = vec2(float(paletteIndex) / 255.0, float(paletteLine) / 255.0);

    return texture2D(paletteTexture, paletteTextureCoords);
}

vec4 hatch_sampleTexture2D(sampler2D texture, vec2 textureCoords, int numPaletteIndices) {
    vec4 finalColor = texture2D(texture, textureCoords);

    if (numPaletteIndices != 0) {
        int paletteIndex = int(finalColor.r * 255.0);
        int paletteLine = hatch_getPaletteLine(u_paletteID);

        paletteIndex = clamp(paletteIndex, 0, numPaletteIndices - 1);

        return hatch_getColorFromPalette(paletteIndex, u_paletteTexture, paletteLine);
    }

    return finalColor;
}
)";
}

char* GLShader::FindInclude(std::string identifier) {
	std::unordered_map<std::string, const char*>::iterator it = shaderIncludes.find(identifier);
	if (it != shaderIncludes.end()) {
		return StringUtils::Duplicate(it->second);
	}

	return nullptr;
}

void GLShader::AddVertexProgram(Stream* stream) {
	GLint compiled = GL_FALSE;

	size_t lenVS = stream->Length();

	char* sourceVS = (char*)Memory::Malloc(lenVS + 1);
	if (sourceVS == nullptr) {
		throw std::runtime_error("No free memory to allocate vertex shader text!");
	}

	stream->ReadBytes(sourceVS, lenVS);
	sourceVS[lenVS] = '\0';

	VertexProgramID = glCreateShader(GL_VERTEX_SHADER);

	glShaderSource(VertexProgramID, 1, &sourceVS, NULL);
	glCompileShader(VertexProgramID);
	CHECK_GL();
	glGetShaderiv(VertexProgramID, GL_COMPILE_STATUS, &compiled);

	Memory::Free(sourceVS);

	if (compiled != GL_TRUE) {
		std::string error = CheckShaderError(VertexProgramID);

		glDeleteShader(VertexProgramID);
		VertexProgramID = 0;

		throw std::runtime_error("Unable to compile vertex shader!\n" + error);
	}
}

GL_ProcessedShader GLShader::ProcessFragmentShaderText(char* text) {
	GL_ProcessedShader output;
	output.SourceText = text;

	char* ptr = text;

	while (*ptr != '\0') {
		// Skip comments
		if (ptr[0] == '/' && ptr[1] == '/') {
			ptr += 2;

			while (*ptr != '\0' && *ptr != '\n') {
				ptr++;
			}

			if (*ptr == '\0') {
				break;
			}
			else if (*ptr == '\n') {
				ptr++;
				continue;
			}
		}
		// Skip multiline comments
		else if (ptr[0] == '/' && ptr[1] == '*') {
			ptr += 2;

			char* end = strstr(ptr, "*/");
			if (!end) {
				break;
			}

			ptr = end + 2;

			if (*ptr == '\0') {
				break;
			}
			else if (*ptr == '\n') {
				ptr++;
				continue;
			}
		}

		if (*ptr != '#') {
			int spn = strspn(ptr, " \r\t");
			if (spn == 0) {
				while (*ptr != '\0' && *ptr != '\n') {
					ptr++;
				}
			}

			if (*ptr == '\0') {
				break;
			}
			else if (*ptr == '\n') {
				ptr++;
				continue;
			}

			ptr += spn;
		}

		if (StringUtils::StartsWith(ptr, "#define ")) {
			char* start = ptr + 8;
			ptr = start;
			while (*ptr != ' ' && *ptr != '\r' && *ptr != '\t' && *ptr != '\n' &&
				*ptr != '\0') {
				ptr++;
			}

			size_t length = ptr - start;
			std::string found = std::string(start, length);
			output.Defines.push_back(found);
		}

		while (*ptr != '\0' && *ptr != '\n') {
			ptr++;
		}

		if (*ptr == '\n') {
			ptr++;
		}
	}

	return output;
}

std::vector<char*> GLShader::GetShaderSources(GL_ProcessedShader processed) {
	std::vector<char*> shaderSources;

#if GL_ES_VERSION_2_0 || GL_ES_VERSION_3_0
	std::string precision = "precision mediump float;\n";
	shaderSources.push_back(StringUtils::Create(precision));
#endif

	for (size_t i = 0; i < processed.Defines.size(); i++) {
		char* includeText = GLShader::FindInclude(processed.Defines[i]);
		if (includeText != nullptr) {
			shaderSources.push_back(includeText);
		}
	}

	std::string lineText = "#line 0\n"; // TODO: Handle different GLSL versions properly here.
	shaderSources.push_back(StringUtils::Create(lineText));

	shaderSources.push_back(processed.SourceText);

	return shaderSources;
}

void GLShader::AddFragmentProgram(Stream* stream) {
	GLint compiled = GL_FALSE;

	size_t lenFS = stream->Length();

	char* sourceFS = (char*)Memory::Malloc(lenFS + 1);
	if (sourceFS == nullptr) {
		throw std::runtime_error("No free memory to allocate fragment shader text!");
	}

	stream->ReadBytes(sourceFS, lenFS);
	sourceFS[lenFS] = '\0';

	GL_ProcessedShader processed = ProcessFragmentShaderText(sourceFS);
	std::vector<char*> shaderSources = GetShaderSources(processed);

	FragmentProgramID = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(FragmentProgramID, shaderSources.size(), shaderSources.data(), NULL);
	glCompileShader(FragmentProgramID);
	CHECK_GL();

	for (size_t i = 0; i < shaderSources.size(); i++) {
		Memory::Free(shaderSources[i]);
	}

	glGetShaderiv(FragmentProgramID, GL_COMPILE_STATUS, &compiled);
	if (compiled != GL_TRUE) {
		std::string error = CheckShaderError(FragmentProgramID);

		glDeleteShader(FragmentProgramID);
		FragmentProgramID = 0;

		throw std::runtime_error("Unable to compile fragment shader!\n" + error);
	}
}

void GLShader::AddProgram(int program, Stream* stream) {
	if (HasProgram(program)) {
		throw std::runtime_error("Shader already has program!");
	}

	switch (program) {
	case Shader::PROGRAM_VERTEX:
		AddVertexProgram(stream);
		break;
	case Shader::PROGRAM_FRAGMENT:
		AddFragmentProgram(stream);
		break;
	default:
		throw std::runtime_error("Unsupported shader program!");
	}
}

bool GLShader::HasProgram(int program) {
	switch (program) {
	case Shader::PROGRAM_VERTEX:
		return VertexProgramID != 0;
	case Shader::PROGRAM_FRAGMENT:
		return FragmentProgramID != 0;
	default:
		return false;
	}
}
bool GLShader::CanCompile() {
	if (WasCompiled()) {
		return false;
	}

	return HasProgram(PROGRAM_VERTEX) && HasProgram(PROGRAM_FRAGMENT);
}
bool GLShader::IsValid() {
	return HasProgram(PROGRAM_VERTEX) && HasProgram(PROGRAM_FRAGMENT) && ProgramID != 0;
}

void GLShader::Compile() {
	if (WasCompiled()) {
		throw std::runtime_error("Shader has already been compiled!");
	}

	ValidatePrograms();

	ProgramID = glCreateProgram();

	try {
		AttachAndLink();
	} catch (const std::runtime_error& error) {
		glDeleteProgram(ProgramID);
		ProgramID = 0;

		throw;
	}

	Compiled = true;
}

GLShader::GLShader() {
	Compiled = false;

#if GL_USING_ATTRIB_LOCATIONS
	InitAttributes();
#endif
	InitTextureUniforms();
}
GLShader::GLShader(std::string vertexShaderSource, std::string fragmentShaderSource) {
	Compiled = false;

	TextStream* streamVS = TextStream::New(vertexShaderSource);
	try {
		AddVertexProgram(streamVS);
	} catch (const std::runtime_error& error) {
		Delete();
		throw;
	}
	streamVS->Close();

	TextStream* streamFS = TextStream::New(fragmentShaderSource);
	try {
		AddFragmentProgram(streamFS);
	} catch (const std::runtime_error& error) {
		Delete();
		throw;
	}
	streamFS->Close();

#if GL_USING_ATTRIB_LOCATIONS
	InitAttributes();
#endif
	InitTextureUniforms();

	try {
		Compile();
	} catch (const std::runtime_error& error) {
		Delete();
		throw;
	}
}
GLShader::GLShader(Stream* streamVS, Stream* streamFS) {
	Compiled = false;

	try {
		AddVertexProgram(streamVS);
	} catch (const std::runtime_error& error) {
		Delete();
		throw;
	}

	try {
		AddFragmentProgram(streamFS);
	} catch (const std::runtime_error& error) {
		Delete();
		throw;
	}

#if GL_USING_ATTRIB_LOCATIONS
	InitAttributes();
#endif
	InitTextureUniforms();

	try {
		Compile();
	} catch (const std::runtime_error& error) {
		Delete();
		throw;
	}
}
void GLShader::AttachAndLink() {
	glAttachShader(ProgramID, VertexProgramID);
	glAttachShader(ProgramID, FragmentProgramID);

#if GL_USING_ATTRIB_LOCATIONS
	BindAttribLocations();
#endif

	glLinkProgram(ProgramID);
	CHECK_GL();

	GLint isLinked = GL_FALSE;
	glGetProgramiv(ProgramID, GL_LINK_STATUS, (int*)&isLinked);
	if (isLinked != GL_TRUE) {
		std::string error = CheckProgramError(ProgramID);

		throw std::runtime_error("Unable to link shader program!\n" + error);
	}

	LocPosition = GetRequiredAttrib(ATTRIB_POSITION);
	LocTexCoord = GetAttribLocation(ATTRIB_UV);
	LocVaryingColor = GetAttribLocation(ATTRIB_COLOR);

	InitUniforms();

	LocProjectionMatrix = AddBuiltinUniform("u_projectionMatrix");
	LocViewMatrix = AddBuiltinUniform("u_viewMatrix");
	LocModelMatrix = AddBuiltinUniform("u_modelMatrix");

	LocColor = AddBuiltinUniform("u_color");
	LocDiffuseColor = AddBuiltinUniform("u_diffuseColor");
	LocSpecularColor = AddBuiltinUniform("u_specularColor");
	LocAmbientColor = AddBuiltinUniform("u_ambientColor");
	LocTexture = AddBuiltinUniform(UNIFORM_TEXTURE);
	LocTextureSize = AddBuiltinUniform("u_textureSize");
	LocSpriteFrameCoords = AddBuiltinUniform("u_spriteFrameCoords");
	LocSpriteFrameSize = AddBuiltinUniform("u_spriteFrameSize");
#ifdef GL_HAVE_YUV
	LocTextureU = AddBuiltinUniform(UNIFORM_TEXTUREU);
	LocTextureV = AddBuiltinUniform(UNIFORM_TEXTUREV);
#endif
	LocPaletteTexture = AddBuiltinUniform(UNIFORM_PALETTETEXTURE);
	LocPaletteID = AddBuiltinUniform("u_paletteID");
	LocPaletteIndexTable = AddBuiltinUniform("u_paletteIndexTable");
	LocNumTexturePaletteIndices = AddBuiltinUniform("u_numTexturePaletteIndices");

	LocFogColor = AddBuiltinUniform("u_fogColor");
	LocFogLinearStart = AddBuiltinUniform("u_fogLinearStart");
	LocFogLinearEnd = AddBuiltinUniform("u_fogLinearEnd");
	LocFogDensity = AddBuiltinUniform("u_fogDensity");
	LocFogTable = AddBuiltinUniform("u_fogTable");

	ValidateTextureUniformNames();
	InitTextureUnitMap();

	Use();
	for (auto it = TextureUnitMap.begin(); it != TextureUnitMap.end(); it++) {
		glUniform1i(it->first, it->second);
	}
	GLRenderer::SetCurrentProgram(GLRenderer::GetCurrentProgram());

	memset(CachedBlendColors, 0, sizeof(CachedBlendColors));
}

std::string GLShader::CheckShaderError(GLuint shader) {
	int infoLogLength = 0;
	int maxLength = infoLogLength;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
	CHECK_GL();

	char* infoLog = new char[maxLength];
	glGetShaderInfoLog(shader, maxLength, &infoLogLength, infoLog);
	infoLog[strlen(infoLog) - 1] = 0;

	std::string infoLogString;
	if (infoLogLength > 0) {
		infoLogString = std::string(infoLog);
	}

	delete[] infoLog;
	return infoLogString;
}
std::string GLShader::CheckProgramError(GLuint prog) {
	int infoLogLength = 0;
	int maxLength = infoLogLength;

	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &maxLength);
	CHECK_GL();

	char* infoLog = new char[maxLength];
	glGetProgramInfoLog(prog, maxLength, &infoLogLength, infoLog);
	infoLog[strlen(infoLog) - 1] = 0;

	std::string infoLogString;
	if (infoLogLength > 0) {
		infoLogString = std::string(infoLog);
	}

	delete[] infoLog;
	return infoLogString;
}

void GLShader::Use() {
	if (!IsValid()) {
		throw std::runtime_error("Shader is not valid!");
	}

	GLRenderer::SetCurrentProgram(ProgramID);
}

void GLShader::Validate() {
	ValidatePrograms();

	if (ProgramID == 0) {
		throw std::runtime_error("Shader has not been compiled!");
	}
}

void GLShader::ValidatePrograms() {
	if (!HasProgram(PROGRAM_VERTEX)) {
		throw std::runtime_error("Shader has no vertex program!");
	}
	else if (!HasProgram(PROGRAM_FRAGMENT)) {
		throw std::runtime_error("Shader has no fragment program!");
	}
}

bool GLShader::HasUniform(const char* name) {
	if (!WasCompiled()) {
		return false;
	}

	return GetUniformLocation(name) != -1;
}
void GLShader::SendUniformValues(int location, size_t count, void* values, Uint8 type) {
	int* valuesInt = (int*)values;
	float* valuesFloat = (float*)values;

	switch (type) {
	case Shader::DATATYPE_INT:
	case Shader::DATATYPE_BOOL:
	case Shader::DATATYPE_SAMPLER_2D:
	case Shader::DATATYPE_SAMPLER_CUBE:
		glUniform1iv(location, count, valuesInt);
		break;
	case Shader::DATATYPE_INT_VEC2:
	case Shader::DATATYPE_BOOL_VEC2:
		glUniform2iv(location, count, valuesInt);
		break;
	case Shader::DATATYPE_INT_VEC3:
	case Shader::DATATYPE_BOOL_VEC3:
		glUniform3iv(location, count, valuesInt);
		break;
	case Shader::DATATYPE_INT_VEC4:
	case Shader::DATATYPE_BOOL_VEC4:
		glUniform4iv(location, count, valuesInt);
		break;
	case Shader::DATATYPE_FLOAT:
		glUniform1fv(location, count, valuesFloat);
		break;
	case Shader::DATATYPE_FLOAT_VEC2:
		glUniform2fv(location, count, valuesFloat);
		break;
	case Shader::DATATYPE_FLOAT_VEC3:
		glUniform3fv(location, count, valuesFloat);
		break;
	case Shader::DATATYPE_FLOAT_VEC4:
		glUniform4fv(location, count, valuesFloat);
		break;
	case Shader::DATATYPE_FLOAT_MAT2:
		glUniformMatrix2fv(location, count, false, valuesFloat);
		break;
	case Shader::DATATYPE_FLOAT_MAT3:
		glUniformMatrix3fv(location, count, false, valuesFloat);
		break;
	case Shader::DATATYPE_FLOAT_MAT4:
		glUniformMatrix4fv(location, count, false, valuesFloat);
		break;
	}
}
void GLShader::SetUniform(ShaderUniform* uniform, size_t count, void* values, Uint8 type) {
	if (type != uniform->Type) {
		throw std::runtime_error("Uniform type mismatch!");
	}

	if (count > 1) {
		if (Shader::DataTypeIsSampler(type)) {
			throw std::runtime_error("Cannot send array to uniform of sampler type!");
		}
		else if (!uniform->IsArray) {
			throw std::runtime_error(
				"Cannot send array to uniform that is not an array!");
		}
	}

	Use();

	SendUniformValues(uniform->Location, count, values, type);
	CHECK_GL();

	GLRenderer::SetCurrentProgram(GLRenderer::GetCurrentProgram());
}
void GLShader::SetUniformTexture(const char* name, Texture* texture) {
	if (name == nullptr || name[0] == '\0') {
		throw std::runtime_error("Invalid uniform name!");
	}

	int uniform = GetUniformLocation(name);
	if (uniform == -1) {
		throw std::runtime_error("No uniform named \"" + std::string(name) + "\"!");
	}

	if (texture != nullptr && texture->DriverData == nullptr) {
		throw std::runtime_error("Invalid texture!");
	}

	int textureUnit = GetTextureUnit(uniform);
	if (textureUnit == -1) {
		throw std::runtime_error(
			"No texture unit assigned to uniform \"" + std::string(name) + "\"!");
	}

	Use();

	GLRenderer::BindTexture(texture, textureUnit, uniform);

	GLRenderer::SetTextureUnit(0);
	GLRenderer::SetCurrentProgram(GLRenderer::GetCurrentProgram());
}
void GLShader::SetUniformTexture(int uniform, int textureID) {
	if (uniform == -1) {
		throw std::runtime_error("Invalid uniform!");
	}

	int textureUnit = GetTextureUnit(uniform);
	if (textureUnit == -1) {
		throw std::runtime_error("No texture unit assigned to uniform!");
	}

	Use();

	GLRenderer::BindTexture(textureID, textureUnit, uniform);

	GLRenderer::SetTextureUnit(0);
	GLRenderer::SetCurrentProgram(GLRenderer::GetCurrentProgram());
}

int GLShader::GetAttribLocation(std::string identifier) {
#if GL_USING_ATTRIB_LOCATIONS
	std::unordered_map<std::string, int>::iterator it = AttribLocationMap.find(identifier);
	if (it != AttribLocationMap.end()) {
		return it->second;
	}

	return -1;
#else
	GLVariableMap::iterator it = AttribMap.find(identifier);
	if (it != AttribMap.end()) {
		return it->second;
	}

	GLint value = glGetAttribLocation(ProgramID, (const GLchar*)identifier.c_str());
	if (value == -1) {
		return -1;
	}

	AttribMap[identifier] = (int)value;

	return (int)value;
#endif
}
int GLShader::GetRequiredAttrib(std::string identifier) {
	int location = GetAttribLocation(identifier);
	if (location == -1) {
		throw std::runtime_error("Unable to find attribute \"" + identifier + "\"!");
	}
	return location;
}
int GLShader::GetUniformLocation(std::string identifier) {
	ShaderUniform* uniform = GetUniform(identifier);
	if (uniform != nullptr) {
		return uniform->Location;
	}

	return -1;
}

void GLShader::Delete() {
	if (VertexProgramID) {
		glDeleteShader(VertexProgramID);
		VertexProgramID = 0;
	}
	if (FragmentProgramID) {
		glDeleteShader(FragmentProgramID);
		FragmentProgramID = 0;
	}
	if (ProgramID) {
		glDeleteProgram(ProgramID);
		ProgramID = 0;
	}

	Shader::Delete();
}

Uint8 GLShader::ConvertDataTypeToEnum(GLenum type) {
	switch (type) {
	case GL_FLOAT:
		return Shader::DATATYPE_FLOAT;
	case GL_FLOAT_VEC2:
		return Shader::DATATYPE_FLOAT_VEC2;
	case GL_FLOAT_VEC3:
		return Shader::DATATYPE_FLOAT_VEC3;
	case GL_FLOAT_VEC4:
		return Shader::DATATYPE_FLOAT_VEC4;
	case GL_INT:
		return Shader::DATATYPE_INT;
	case GL_INT_VEC2:
		return Shader::DATATYPE_INT_VEC2;
	case GL_INT_VEC3:
		return Shader::DATATYPE_INT_VEC3;
	case GL_INT_VEC4:
		return Shader::DATATYPE_INT_VEC4;
	case GL_BOOL:
		return Shader::DATATYPE_BOOL;
	case GL_BOOL_VEC2:
		return Shader::DATATYPE_BOOL_VEC2;
	case GL_BOOL_VEC3:
		return Shader::DATATYPE_BOOL_VEC3;
	case GL_BOOL_VEC4:
		return Shader::DATATYPE_BOOL_VEC4;
	case GL_FLOAT_MAT2:
		return Shader::DATATYPE_FLOAT_MAT2;
	case GL_FLOAT_MAT3:
		return Shader::DATATYPE_FLOAT_MAT3;
	case GL_FLOAT_MAT4:
		return Shader::DATATYPE_FLOAT_MAT4;
	case GL_SAMPLER_2D:
		return Shader::DATATYPE_SAMPLER_2D;
	case GL_SAMPLER_CUBE:
		return Shader::DATATYPE_SAMPLER_CUBE;
	default:
		return Shader::DATATYPE_UNKNOWN;
	}
}

void GLShader::InitUniforms() {
	GLint numUniforms;
	glGetProgramiv(ProgramID, GL_ACTIVE_UNIFORMS, &numUniforms);

	GLint maxUniformNameLength;
	glGetProgramiv(ProgramID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);

	if (maxUniformNameLength == 0) {
		throw std::runtime_error(
			"GL_ACTIVE_UNIFORM_MAX_LENGTH returned an invalid length!");
	}

	GLchar* uniformName = (GLchar*)Memory::Calloc(maxUniformNameLength, sizeof(GLchar));
	GLsizei nameLength;
	GLint size;
	GLenum type;

	for (GLint i = 0; i < numUniforms; i++) {
		glGetActiveUniform(
			ProgramID, i, maxUniformNameLength, &nameLength, &size, &type, uniformName);
		if (nameLength == 0) {
			continue;
		}

		bool isArray = false;

		char* leftSquareBracket = strchr(uniformName, '[');
		if (leftSquareBracket != nullptr) {
			*leftSquareBracket = '\0';
			isArray = true;
		}

		ShaderUniform uniform;
		uniform.Name = std::string(uniformName, nameLength);
		uniform.Type = ConvertDataTypeToEnum(type);
		uniform.Location =
			glGetUniformLocation(ProgramID, (const GLchar*)uniform.Name.c_str());
		uniform.IsArray = isArray;

		UniformMap[uniformName] = uniform;
	}

	Memory::Free(uniformName);
}

void GLShader::InitTextureUniforms() {
	AddTextureUniformName(UNIFORM_TEXTURE);
	AddTextureUniformName(UNIFORM_PALETTETEXTURE);

#ifdef GL_HAVE_YUV
	AddTextureUniformName(UNIFORM_TEXTUREU);
	AddTextureUniformName(UNIFORM_TEXTUREV);
#endif
}

#if GL_USING_ATTRIB_LOCATIONS
void GLShader::InitAttributes() {
	AttribLocationMap[ATTRIB_POSITION] = 0;
	AttribLocationMap[ATTRIB_UV] = 1;
	AttribLocationMap[ATTRIB_COLOR] = 2;
}

void GLShader::BindAttribLocations() {
	for (auto it = AttribLocationMap.begin(); it != AttribLocationMap.end(); it++) {
		std::string attribName = it->first;
		int location = it->second;

		glBindAttribLocation(ProgramID, location, attribName.c_str());
	}

	AttribLocationMap.clear();
}
#endif

GLShader::~GLShader() {
	Delete();
}

#endif /* USING_OPENGL */
