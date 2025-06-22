#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ShaderImpl.h>
#include <Engine/Graphics.h>
#include <Engine/Rendering/Shader.h>

ObjClass* ShaderImpl::Class = nullptr;

void ShaderImpl::Init() {
	const char* className = "Shader";

	Class = NewClass(Murmur::EncryptString(className));
	Class->Name = CopyString(className);
	Class->NewFn = VM_New;

	ScriptManager::DefineNative(Class, "HasProgram", VM_HasProgram);
	ScriptManager::DefineNative(Class, "CanCompile", VM_CanCompile);
	ScriptManager::DefineNative(Class, "IsValid", VM_IsValid);
	ScriptManager::DefineNative(Class, "AddProgram", VM_AddProgram);
	ScriptManager::DefineNative(Class, "SetTextureUnit", VM_SetTextureUnit);
	ScriptManager::DefineNative(Class, "GetTextureUnit", VM_GetTextureUnit);
	ScriptManager::DefineNative(Class, "Compile", VM_Compile);
	ScriptManager::DefineNative(Class, "SetUniform", VM_SetUniform);
	ScriptManager::DefineNative(Class, "SetTexture", VM_SetTexture);
	ScriptManager::DefineNative(Class, "Delete", VM_Delete);

	ScriptManager::ClassImplList.push_back(Class);

	ScriptManager::Globals->Put(className, OBJECT_VAL(Class));
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

#define CHECK_EXISTS(ptr) \
	if (ptr == nullptr) { \
		throw ScriptException("Shader has been deleted!"); \
	}

/***
 * \constructor
 * \desc Creates a shader.
 * \ns Shader
 */
Obj* ShaderImpl::VM_New() {
	Shader* shader = Graphics::CreateShader();
	if (shader == nullptr) {
		throw ScriptException("Could not create shader!");
	}

	ObjShader* obj = NewShader((void*)shader);
	shader->Object = (void*)obj;
	return (Obj*)obj;
}
/***
 * \method HasProgram
 * \desc Checks if the shader has a program.
 * \param program (Enum): <linkto ref="SHADERPROGRAM_*">Shader program type</linkto>.
 * \return Returns <code>true</code> if there is a program of the given type, <code>false</code> if otherwise.
 * \ns Shader
 */
VMValue ShaderImpl::VM_HasProgram(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjShader* objShader = GET_ARG(0, GetShader);
	if (objShader == nullptr) {
		return NULL_VAL;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	CHECK_EXISTS(shader);

	int program = GET_ARG(1, GetInteger);

	bool hasProgram = shader->HasProgram(program);

	return INTEGER_VAL(hasProgram);
}
/***
 * \method CanCompile
 * \desc Checks if the shader can be compiled.
 * \return Returns <code>true</code> if the shader can be compiled, <code>false</code> if otherwise.
 * \ns Shader
 */
VMValue ShaderImpl::VM_CanCompile(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjShader* objShader = GET_ARG(0, GetShader);
	if (objShader == nullptr) {
		return NULL_VAL;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	CHECK_EXISTS(shader);

	bool hasProgram = shader->CanCompile();

	return INTEGER_VAL(hasProgram);
}
/***
 * \method IsValid
 * \desc Checks if the shader can be used.
 * \param program (Enum): <linkto ref="SHADERPROGRAM_*">Shader program type</linkto>.
 * \return Returns <code>true</code> if the shader can be used, <code>false</code> if otherwise.
 * \ns Shader
 */
VMValue ShaderImpl::VM_IsValid(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjShader* objShader = GET_ARG(0, GetShader);
	if (objShader == nullptr) {
		return NULL_VAL;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	CHECK_EXISTS(shader);

	bool hasProgram = shader->IsValid();

	return INTEGER_VAL(hasProgram);
}
/***
 * \method AddProgram
 * \desc Adds a program.
 * \param program (Enum): <linkto ref="SHADERPROGRAM_*">Shader program type</linkto>.
 * \param filename (String): Filename of the resource.
 * \ns Shader
 */
VMValue ShaderImpl::VM_AddProgram(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjShader* objShader = GET_ARG(0, GetShader);
	if (objShader == nullptr) {
		return NULL_VAL;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	CHECK_EXISTS(shader);

	int program = GET_ARG(1, GetInteger);
	char* filename = GET_ARG(2, GetString);

	Stream* stream = ResourceStream::New(filename);
	if (!stream) {
		throw ScriptException("Resource \"" + std::string(filename) + "\" does not exist!");
	}

	bool success = false;

	try {
		shader->AddProgram(program, stream);

		success = true;
	} catch (const std::runtime_error& error) {
		std::string errorMessage = "Error adding shader program: ";

		errorMessage += std::string(error.what());

		ScriptManager::Threads[threadID].ShowErrorLocation(errorMessage.c_str());
	}

	stream->Close();

	return INTEGER_VAL(success);
}
/***
 * \method SetTextureUnit
 * \desc Assigns a texture unit to a texture uniform.
 * \param uniform (String): The name of the uniform.
 * \param unit (Integer): The texture unit.
 * \ns Shader
 */
VMValue ShaderImpl::VM_SetTextureUnit(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjShader* objShader = GET_ARG(0, GetShader);
	if (objShader == nullptr) {
		return NULL_VAL;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	CHECK_EXISTS(shader);

	char* uniform = GET_ARG(1, GetString);
	int unit = GET_ARG(2, GetInteger);

	if (shader->WasCompiled()) {
		throw ScriptException("Cannot set texture unit after compilation!");
	}

	try {
		shader->SetTextureUniformUnit(std::string(uniform), unit);
	} catch (const std::runtime_error& error) {
		throw ScriptException(error.what());
	}

	return NULL_VAL;
}
/***
 * \method GetTextureUnit
 * \desc Gets the texture unit assigned to a texture uniform.
 * \param uniform (String): The name of the uniform.
 * \return Returns an Integer value, or <code>null</code> if no texture unit was assigned.
 * \ns Shader
 */
VMValue ShaderImpl::VM_GetTextureUnit(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjShader* objShader = GET_ARG(0, GetShader);
	if (objShader == nullptr) {
		return NULL_VAL;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	CHECK_EXISTS(shader);

	char* uniformName = GET_ARG(1, GetString);

	std::string key = std::string(uniformName);
	std::unordered_map<std::string, int>::iterator it = shader->TextureUniformMap.find(key);
	if (it == shader->TextureUniformMap.end()) {
		return NULL_VAL;
	}

	int textureUnit = it->second;

	return INTEGER_VAL(textureUnit);
}
/***
 * \method Compile
 * \desc Compiles a shader.
 * \ns Shader
 */
VMValue ShaderImpl::VM_Compile(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjShader* objShader = AS_SHADER(args[0]);
	Shader* shader = (Shader*)objShader->ShaderPtr;

	CHECK_EXISTS(shader);

	bool success = false;

	try {
		shader->Compile();

		success = true;
	} catch (const std::runtime_error& error) {
		std::string errorMessage = "Error compiling shader: ";

		errorMessage += std::string(error.what());

		ScriptManager::Threads[threadID].ShowErrorLocation(errorMessage.c_str());
	}

	return INTEGER_VAL(success);
}
/***
 * \method SetUniform
 * \desc Sets the value of an uniform.
 * \param uniform (String): The name of the uniform.
 * \param value (Number or Array): The value to send to the shader.
 * \ns Shader
 */
VMValue ShaderImpl::VM_SetUniform(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjShader* objShader = AS_SHADER(args[0]);
	Shader* shader = (Shader*)objShader->ShaderPtr;

	CHECK_EXISTS(shader);

	char* uniform = GET_ARG(1, GetString);
	float value = GET_ARG(2, GetDecimal);
	// VMValue value = args[1];

	void* values;
	values = Memory::Malloc(sizeof(int) * 1);

	float* valuesFloat = (float*)values;
	valuesFloat[0] = value;
	// valuesFloat[0] = AS_DECIMAL(value);

	try {
		shader->SetUniform(uniform, 1, valuesFloat);
	} catch (const std::runtime_error& error) {
		throw ScriptException(error.what());
	}

	Memory::Free(values);

	return NULL_VAL;
}
/***
 * \method SetTexture
 * \desc Binds a texture to an uniform.
 * \param uniform (String): The name of the uniform.
 * \param texture (Texture): The texture to bind to the uniform.
 * \ns Shader
 */
VMValue ShaderImpl::VM_SetTexture(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjShader* objShader = AS_SHADER(args[0]);
	Shader* shader = (Shader*)objShader->ShaderPtr;

	CHECK_EXISTS(shader);

	char* uniform = GET_ARG(1, GetString);
	Texture* texture = nullptr;

	if (!IS_NULL(args[2])) {
		Image* image = GET_ARG(2, GetImage);
		if (image != nullptr) {
			texture = image->TexturePtr;
		}
	}

	try {
		shader->SetUniformTexture(uniform, texture);
	} catch (const std::runtime_error& error) {
		throw ScriptException(error.what());
	}

	return NULL_VAL;
}
/***
 * \method Delete
 * \desc Deletes a shader.
 * \ns Shader
 */
VMValue ShaderImpl::VM_Delete(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjShader* objShader = AS_SHADER(args[0]);
	Shader* shader = (Shader*)objShader->ShaderPtr;

	CHECK_EXISTS(shader);

	Graphics::DeleteShader(shader);

	objShader->ShaderPtr = nullptr;

	return NULL_VAL;
}

#undef CHECK_EXISTS
#undef GET_ARG
