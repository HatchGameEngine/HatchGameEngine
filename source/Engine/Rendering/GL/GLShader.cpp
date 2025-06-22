#ifdef USING_OPENGL

#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/TextStream.h>
#include <Engine/Rendering/GL/GLRenderer.h>
#include <Engine/Rendering/GL/GLShader.h>

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

void GLShader::AddFragmentProgram(Stream* stream) {
	GLint compiled = GL_FALSE;

	size_t lenFS = stream->Length();

	char* sourceFS = (char*)Memory::Malloc(lenFS + 1);
	if (sourceFS == nullptr) {
		throw std::runtime_error("No free memory to allocate fragment shader text!");
	}

	stream->ReadBytes(sourceFS, lenFS);
	sourceFS[lenFS] = '\0';

#if GL_ES_VERSION_2_0 || GL_ES_VERSION_3_0
	const char* sourceFSMod[2];
	sourceFSMod[0] = "precision mediump float;\n";
	sourceFSMod[1] = sourceFS;
#endif

	FragmentProgramID = glCreateShader(GL_FRAGMENT_SHADER);
#if GL_ES_VERSION_2_0 || GL_ES_VERSION_3_0
	glShaderSource(FragmentProgramID, 2, sourceFSMod, NULL);
#else
	glShaderSource(FragmentProgramID, 1, &sourceFS, NULL);
#endif
	glCompileShader(FragmentProgramID);
	CHECK_GL();
	glGetShaderiv(FragmentProgramID, GL_COMPILE_STATUS, &compiled);

	Memory::Free(sourceFS);

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
	LocPaletteID = AddBuiltinUniform(UNIFORM_PALETTEID);
	LocPaletteIndexTable = AddBuiltinUniform("u_paletteIndexTable");

	LocFogColor = AddBuiltinUniform("u_fogColor");
	LocFogLinearStart = AddBuiltinUniform("u_fogLinearStart");
	LocFogLinearEnd = AddBuiltinUniform("u_fogLinearEnd");
	LocFogDensity = AddBuiltinUniform("u_fogDensity");
	LocFogTable = AddBuiltinUniform("u_fogTable");

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
void GLShader::SetUniform(const char* name, size_t count, int* values) {
	if (name == nullptr || name[0] == '\0') {
		throw std::runtime_error("Invalid uniform name!");
	}

	int uniform = GetUniformLocation(name);
	if (uniform == -1) {
		throw std::runtime_error("No uniform named \"" + std::string(name) + "\"!");
	}

	Use();

	switch (count) {
	case 1:
		glUniform1i(uniform, values[0]);
		break;
	case 2:
		glUniform2i(uniform, values[0], values[1]);
		break;
	case 3:
		glUniform3i(uniform, values[0], values[1], values[2]);
		break;
	case 4:
		glUniform4i(uniform, values[0], values[1], values[2], values[3]);
		break;
	}

	GLRenderer::SetCurrentProgram(GLRenderer::GetCurrentProgram());
}
void GLShader::SetUniform(const char* name, size_t count, float* values) {
	if (name == nullptr || name[0] == '\0') {
		throw std::runtime_error("Invalid uniform name!");
	}

	int uniform = GetUniformLocation(name);
	if (uniform == -1) {
		throw std::runtime_error("No uniform named \"" + std::string(name) + "\"!");
	}

	Use();

	switch (count) {
	case 1:
		glUniform1f(uniform, values[0]);
		break;
	case 2:
		glUniform2f(uniform, values[0], values[1]);
		break;
	case 3:
		glUniform3f(uniform, values[0], values[1], values[2]);
		break;
	case 4:
		glUniform4f(uniform, values[0], values[1], values[2], values[3]);
		break;
	}

	GLRenderer::SetCurrentProgram(GLRenderer::GetCurrentProgram());
}
void GLShader::SetUniformArray(const char* name, size_t count, int* values, size_t numValues) {
	if (name == nullptr || name[0] == '\0') {
		throw std::runtime_error("Invalid uniform name!");
	}

	int uniform = GetUniformLocation(name);
	if (uniform == -1) {
		throw std::runtime_error("No uniform named \"" + std::string(name) + "\"!");
	}

	Use();

	switch (count) {
	case 1:
		glUniform1iv(uniform, numValues, values);
		break;
	case 2:
		glUniform2iv(uniform, numValues, values);
		break;
	case 3:
		glUniform3iv(uniform, numValues, values);
		break;
	case 4:
		glUniform4iv(uniform, numValues, values);
		break;
	}

	GLRenderer::SetCurrentProgram(GLRenderer::GetCurrentProgram());
}
void GLShader::SetUniformArray(const char* name, size_t count, float* values, size_t numValues) {
	if (name == nullptr || name[0] == '\0') {
		throw std::runtime_error("Invalid uniform name!");
	}

	int uniform = GetUniformLocation(name);
	if (uniform == -1) {
		throw std::runtime_error("No uniform named \"" + std::string(name) + "\"!");
	}

	Use();

	switch (count) {
	case 1:
		glUniform1fv(uniform, numValues, values);
		break;
	case 2:
		glUniform2fv(uniform, numValues, values);
		break;
	case 3:
		glUniform3fv(uniform, numValues, values);
		break;
	case 4:
		glUniform4fv(uniform, numValues, values);
		break;
	}

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
		throw std::runtime_error("No texture unit assigned to uniform \"" + std::string(name) + "\"!");
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
	GLVariableMap::iterator it = UniformMap.find(identifier);
	if (it != UniformMap.end()) {
		return it->second;
	}

	GLint value = glGetUniformLocation(ProgramID, (const GLchar*)identifier.c_str());
	if (value == -1) {
		return -1;
	}

	UniformMap[identifier] = (int)value;

	return (int)value;
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

#if GL_USING_ATTRIB_LOCATIONS
void GLShader::InitAttributes() {
	AttribLocationMap[ATTRIB_POSITION] = 0;
	AttribLocationMap[ATTRIB_UV] = 1;
	AttribLocationMap[ATTRIB_COLOR] = 2;
}
#endif

void GLShader::InitTextureUniforms() {
	AddTextureUniformName(UNIFORM_TEXTURE);
	AddTextureUniformName(UNIFORM_PALETTETEXTURE);

#ifdef GL_HAVE_YUV
	AddTextureUniformName(UNIFORM_TEXTUREU);
	AddTextureUniformName(UNIFORM_TEXTUREV);
#endif
}

#if GL_USING_ATTRIB_LOCATIONS
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
