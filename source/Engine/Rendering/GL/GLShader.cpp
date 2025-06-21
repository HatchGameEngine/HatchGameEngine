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

bool GLShader::IsValid() {
	return HasProgram(PROGRAM_VERTEX) && HasProgram(PROGRAM_FRAGMENT) && ProgramID != 0;
}

void GLShader::Compile() {
	ProgramID = glCreateProgram();

	if (!AttachAndLink()) {
		std::string error = CheckProgramError(ProgramID);

		glDeleteProgram(ProgramID);
		ProgramID = 0;

		throw std::runtime_error("Unable to link shader program!\n" + error);
	}
}

GLShader::GLShader() {}
GLShader::GLShader(std::string vertexShaderSource, std::string fragmentShaderSource) {
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

	try {
		Compile();
	} catch (const std::runtime_error& error) {
		Delete();
		throw;
	}
}
GLShader::GLShader(Stream* streamVS, Stream* streamFS) {
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

	try {
		Compile();
	} catch (const std::runtime_error& error) {
		Delete();
		throw;
	}
}
bool GLShader::AttachAndLink() {
	glAttachShader(ProgramID, VertexProgramID);
	glAttachShader(ProgramID, FragmentProgramID);

	glBindAttribLocation(ProgramID, 0, "i_position");
	glBindAttribLocation(ProgramID, 1, "i_uv");
	glBindAttribLocation(ProgramID, 2, "i_color");

	glLinkProgram(ProgramID);
	CHECK_GL();

	GLint isLinked = GL_FALSE;
	glGetProgramiv(ProgramID, GL_LINK_STATUS, (int*)&isLinked);
	if (isLinked != GL_TRUE) {
		return false;
	}

	LocProjectionMatrix = GetUniformLocation("u_projectionMatrix");
	LocViewMatrix = GetUniformLocation("u_viewMatrix");
	LocModelMatrix = GetUniformLocation("u_modelMatrix");

	LocPosition = GetAttribLocation("i_position");
	LocTexCoord = GetAttribLocation("i_uv");
	LocVaryingColor = GetAttribLocation("i_color");

	LocColor = GetUniformLocation("u_color");
	LocDiffuseColor = GetUniformLocation("u_diffuseColor");
	LocSpecularColor = GetUniformLocation("u_specularColor");
	LocAmbientColor = GetUniformLocation("u_ambientColor");
	LocTexture = GetUniformLocation("u_texture");
	LocTextureSize = GetUniformLocation("u_textureSize");
	LocSpriteFrameCoords = GetUniformLocation("u_spriteFrameCoords");
	LocSpriteFrameSize = GetUniformLocation("u_spriteFrameSize");
#ifdef GL_HAVE_YUV
	LocTextureU = GetUniformLocation("u_textureU");
	LocTextureV = GetUniformLocation("u_textureV");
#endif
	LocPaletteTexture = GetUniformLocation("u_paletteTexture");
	LocPaletteLine = GetUniformLocation("u_paletteLine");
	LocPaletteIndexTable = GetUniformLocation("u_paletteIndexTable");

	LocFogColor = GetUniformLocation("u_fogColor");
	LocFogLinearStart = GetUniformLocation("u_fogLinearStart");
	LocFogLinearEnd = GetUniformLocation("u_fogLinearEnd");
	LocFogDensity = GetUniformLocation("u_fogDensity");
	LocFogTable = GetUniformLocation("u_fogTable");

	CachedBlendColors[0] = CachedBlendColors[1] = CachedBlendColors[2] = CachedBlendColors[3] =
		0.0;

	return true;
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

	glUseProgram(ProgramID);
	CHECK_GL();
}

void GLShader::Validate() {
	if (!HasProgram(PROGRAM_VERTEX)) {
		throw std::runtime_error("Shader has no vertex program!");
	}
	else if (!HasProgram(PROGRAM_FRAGMENT)) {
		throw std::runtime_error("Shader has no fragment program!");
	}
	else if (ProgramID == 0) {
		throw std::runtime_error("Shader has not been compiled!");
	}
}

bool GLShader::HasUniform(const char* name) {
	GLint location = GetUniformLocation(name);
	return location != -1;
}
void GLShader::SetUniform(const char* name, size_t count, int* values) {
	if (name == nullptr || name[0] == '\0') {
		throw std::runtime_error("Invalid uniform name!");
	}

	GLint location = GetUniformLocation(name);
	if (location == -1) {
		throw std::runtime_error("No uniform named \"" + std::string(name) + "\"!");
	}

	switch (count) {
	case 1:
		glUniform1i(location, values[0]);
		break;
	case 2:
		glUniform2i(location, values[0], values[1]);
		break;
	case 3:
		glUniform3i(location, values[0], values[1], values[2]);
		break;
	case 4:
		glUniform4i(location, values[0], values[1], values[2], values[3]);
		break;
	}
}
void GLShader::SetUniform(const char* name, size_t count, float* values) {
	if (name == nullptr || name[0] == '\0') {
		throw std::runtime_error("Invalid uniform name!");
	}

	GLint location = GetUniformLocation(name);
	if (location == -1) {
		throw std::runtime_error("No uniform named \"" + std::string(name) + "\"!");
	}

	switch (count) {
	case 1:
		glUniform1f(location, values[0]);
		break;
	case 2:
		glUniform2f(location, values[0], values[1]);
		break;
	case 3:
		glUniform3f(location, values[0], values[1], values[2]);
		break;
	case 4:
		glUniform4f(location, values[0], values[1], values[2], values[3]);
		break;
	}
}
void GLShader::SetUniformArray(const char* name, size_t count, int* values, size_t numValues) {
	if (name == nullptr || name[0] == '\0') {
		throw std::runtime_error("Invalid uniform name!");
	}

	GLint location = GetUniformLocation(name);
	if (location == -1) {
		throw std::runtime_error("No uniform named \"" + std::string(name) + "\"!");
	}

	switch (count) {
	case 1:
		glUniform1iv(location, numValues, values);
		break;
	case 2:
		glUniform2iv(location, numValues, values);
		break;
	case 3:
		glUniform3iv(location, numValues, values);
		break;
	case 4:
		glUniform4iv(location, numValues, values);
		break;
	}
}
void GLShader::SetUniformArray(const char* name, size_t count, float* values, size_t numValues) {
	if (name == nullptr || name[0] == '\0') {
		throw std::runtime_error("Invalid uniform name!");
	}

	GLint location = GetUniformLocation(name);
	if (location == -1) {
		throw std::runtime_error("No uniform named \"" + std::string(name) + "\"!");
	}

	switch (count) {
	case 1:
		glUniform1fv(location, numValues, values);
		break;
	case 2:
		glUniform2fv(location, numValues, values);
		break;
	case 3:
		glUniform3fv(location, numValues, values);
		break;
	case 4:
		glUniform4fv(location, numValues, values);
		break;
	}
}
void GLShader::SetUniformTexture(const char* name, Texture* texture, int textureUnit) {
	if (name == nullptr || name[0] == '\0') {
		throw std::runtime_error("Invalid uniform name!");
	}

	GLint location = GetUniformLocation(name);
	if (location == -1) {
		throw std::runtime_error("No uniform named \"" + std::string(name) + "\"!");
	}

	if (texture == nullptr || texture->DriverData == nullptr) {
		throw std::runtime_error("Invalid texture!");
	}

	GLRenderer::BindTexture(texture, textureUnit, (int)location);
}

GLint GLShader::GetAttribLocation(std::string identifier) {
	GLVariableMap::iterator it = AttribMap.find(identifier);
	if (it != AttribMap.end()) {
		return it->second;
	}

	GLint value = glGetAttribLocation(ProgramID, (const GLchar*)identifier.c_str());
	if (value == -1) {
		return -1;
	}

	AttribMap[identifier] = (int)value;

	return value;
}
GLint GLShader::GetUniformLocation(std::string identifier) {
	GLVariableMap::iterator it = UniformMap.find(identifier);
	if (it != UniformMap.end()) {
		return it->second;
	}

	GLint value = glGetUniformLocation(ProgramID, (const GLchar*)identifier.c_str());
	if (value == -1) {
		return -1;
	}

	UniformMap[identifier] = (int)value;

	return value;
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

GLShader::~GLShader() {
	Delete();
}

#endif /* USING_OPENGL */
