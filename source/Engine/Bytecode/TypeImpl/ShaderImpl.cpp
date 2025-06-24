#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ShaderImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Graphics.h>
#include <Engine/Rendering/Shader.h>

static size_t GetArrayUniformValues(ObjArray* array, void** values, Uint8 uniformType);
static void
ThrowElementTypeMismatchError(size_t element, const char* expected, const char* elementType);

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
	ScriptManager::DefineNative(Class, "AssignTextureUnit", VM_AssignTextureUnit);
	ScriptManager::DefineNative(Class, "GetTextureUnit", VM_GetTextureUnit);
	ScriptManager::DefineNative(Class, "Compile", VM_Compile);
	ScriptManager::DefineNative(Class, "HasUniform", VM_HasUniform);
	ScriptManager::DefineNative(Class, "SetUniform", VM_SetUniform);
	ScriptManager::DefineNative(Class, "SetTexture", VM_SetTexture);
	ScriptManager::DefineNative(Class, "Delete", VM_Delete);

	ScriptManager::ClassImplList.push_back(Class);

	ScriptManager::Globals->Put(className, OBJECT_VAL(Class));
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, StandardLibrary::argFunction) : argDefault)

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
 * \desc Checks if the shader has a program of a given type.
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
	int program = GET_ARG(1, GetInteger);

	CHECK_EXISTS(shader);

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
 * \desc Adds a program. This should not be called multiple times for the same shader program type.
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
	int program = GET_ARG(1, GetInteger);
	char* filename = GET_ARG(2, GetString);

	CHECK_EXISTS(shader);

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
 * \method AssignTextureUnit
 * \desc Assigns a texture unit to a texture uniform. This is safe to call multiple times for the same uniform name.
 * \param uniform (String): The name of the uniform.
 * \ns Shader
 */
VMValue ShaderImpl::VM_AssignTextureUnit(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjShader* objShader = GET_ARG(0, GetShader);
	if (objShader == nullptr) {
		return NULL_VAL;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	char* uniform = GET_ARG(1, GetString);

	CHECK_EXISTS(shader);

	if (shader->WasCompiled()) {
		throw ScriptException("Cannot assign texture unit after compilation!");
	}

	shader->AddTextureUniformName(std::string(uniform));

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
	char* uniformName = GET_ARG(1, GetString);

	CHECK_EXISTS(shader);

	if (!shader->WasCompiled()) {
		throw ScriptException("Cannot get texture unit before compilation!");
	}

	std::string identifier(uniformName);

	int uniform = shader->GetUniformLocation(identifier);
	if (uniform == -1) {
		throw ScriptException("No uniform named \"" + identifier + "\"!");
	}

	int textureUnit = shader->GetTextureUnit(uniform);
	if (textureUnit == -1) {
		return NULL_VAL;
	}

	return INTEGER_VAL(textureUnit);
}
/***
 * \method Compile
 * \desc Compiles a shader.
 * \return Returns <code>true</code> if the shader was compiled, <code>false</code> if otherwise.
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
 * \method HasUniform
 * \desc Checks if the shader has an uniform with the given name. This can only be called after the shader has been compiled.
 * \param uniform (String): The name of the uniform.
 * \return Returns <code>true</code> if there is an uniform with the given name, <code>false</code> if otherwise.
 * \ns Shader
 */
VMValue ShaderImpl::VM_HasUniform(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjShader* objShader = AS_SHADER(args[0]);
	Shader* shader = (Shader*)objShader->ShaderPtr;
	char* uniform = GET_ARG(1, GetString);

	CHECK_EXISTS(shader);

	if (!shader->WasCompiled()) {
		throw ScriptException("Cannot check for uniform existence before compilation!");
	}

	return INTEGER_VAL(shader->HasUniform(uniform));
}
/***
 * \method SetUniform
 * \desc Sets the value of an uniform variable. The value passed to the uniform persists between different invocations, and can be set at any time, as long as the shader is valid. This can only be called after the shader has been compiled.
 * \param uniform (String): The name of the uniform.
 * \param value (Number or Array): The value.
 * \paramOpt type (Enum): The type of the value being sent. This is required if the value being passed is an Array.
 * \ns Shader
 */
VMValue ShaderImpl::VM_SetUniform(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 3);

	ObjShader* objShader = AS_SHADER(args[0]);
	char* uniformName = GET_ARG(1, GetString);
	VMValue value = args[2];
	int uniformType = GET_ARG_OPT(3, GetInteger, Shader::UNIFORM_UNKNOWN);

	Shader* shader = (Shader*)objShader->ShaderPtr;
	CHECK_EXISTS(shader);

	if (!shader->WasCompiled()) {
		throw ScriptException("Cannot set uniform before compilation!");
	}

	// Validate input
	if (uniformName[0] == '\0') {
		throw ScriptException("Invalid uniform name!");
	}

	ShaderUniform* uniform = shader->GetUniform(uniformName);
	if (uniform == nullptr) {
		throw ScriptException("No uniform named \"" + std::string(uniformName) + "\"!");
	}

	if (uniformType == Shader::UNIFORM_SAMPLER_2D ||
		uniformType == Shader::UNIFORM_SAMPLER_CUBE) {
		throw ScriptException(
			"Cannot change texture unit! Use AssignTextureUnit before compilation.");
	}

	// Check type of value
	bool isArrayType = IS_ARRAY(value);
	bool typeMismatch = false;

	Uint8 type;

	if (IS_INTEGER(value)) {
		// Lets you pass an int to an uniform that is expecting a float.
		if (uniform->Type == Shader::UNIFORM_FLOAT) {
			type = Shader::UNIFORM_FLOAT;
			value = Value::CastAsDecimal(value);
		}
		else {
			type = Shader::UNIFORM_INT;
		}

		if (uniformType != Shader::UNIFORM_UNKNOWN && uniformType != Shader::UNIFORM_INT) {
			if (uniformType == Shader::UNIFORM_FLOAT) {
				value = Value::CastAsDecimal(value);
			}
			else {
				typeMismatch = true;
			}
		}
	}
	else if (IS_DECIMAL(value)) {
		type = Shader::UNIFORM_FLOAT;

		if (uniformType != Shader::UNIFORM_UNKNOWN &&
			uniformType != Shader::UNIFORM_FLOAT) {
			if (uniformType == Shader::UNIFORM_INT) {
				value = Value::CastAsInteger(value);
			}
			else {
				typeMismatch = true;
			}
		}
	}
	else if (isArrayType) {
		if (uniformType == Shader::UNIFORM_UNKNOWN) {
			throw ScriptException("Must specify uniform type if value is an array!");
		}

		switch (uniformType) {
		case Shader::UNIFORM_FLOAT:
		case Shader::UNIFORM_FLOAT_VEC2:
		case Shader::UNIFORM_FLOAT_VEC3:
		case Shader::UNIFORM_FLOAT_VEC4:
		case Shader::UNIFORM_INT:
		case Shader::UNIFORM_INT_VEC2:
		case Shader::UNIFORM_INT_VEC3:
		case Shader::UNIFORM_INT_VEC4:
		case Shader::UNIFORM_BOOL:
		case Shader::UNIFORM_BOOL_VEC2:
		case Shader::UNIFORM_BOOL_VEC3:
		case Shader::UNIFORM_BOOL_VEC4:
		case Shader::UNIFORM_FLOAT_MAT2:
		case Shader::UNIFORM_FLOAT_MAT3:
		case Shader::UNIFORM_FLOAT_MAT4:
			// Okay.
			type = uniformType;
			break;
		default:
			throw ScriptException("Invalid uniform type!");
			break;
		}
	}
	else {
		throw ScriptException("Invalid value type! Must be integer, decimal, or array.");
	}

	if (typeMismatch) {
		throw ScriptException("Uniform type mismatch!");
	}

	// Build the data to send to the shader
	void* values = nullptr;
	size_t count = 1;

	if (isArrayType) {
		count = GetArrayUniformValues(AS_ARRAY(value), &values, uniformType);
	}
	else if (type == Shader::UNIFORM_INT) {
		values = Memory::Malloc(sizeof(int) * count);

		int* valuesInt = (int*)values;
		valuesInt[0] = AS_INTEGER(value);
	}
	else if (type == Shader::UNIFORM_FLOAT) {
		values = Memory::Malloc(sizeof(float) * count);

		float* valuesFloat = (float*)values;
		valuesFloat[0] = AS_DECIMAL(value);
	}
	else {
		UNREACHABLE_EXCEPTION(ScriptException);
	}

	try {
		shader->SetUniform(uniform, count, values, type);
	} catch (const std::runtime_error& error) {
		throw ScriptException(error.what());
	}

	Memory::Free(values);

	return NULL_VAL;
}
size_t GetArrayUniformValues(ObjArray* array, void** values, Uint8 uniformType) {
	size_t arrayLength = array->Values->size();
	size_t numElements = 1;

	char errorString[128];

	if (Shader::UniformTypeIsMatrix(uniformType)) {
		size_t expectedCount = Shader::GetMatrixUniformTypeSize(uniformType);

		if (arrayLength != expectedCount) {
			snprintf(errorString,
				sizeof errorString,
				"Expected array to have exactly %d elements, but it had %d elements instead!",
				expectedCount,
				arrayLength);

			throw ScriptException(std::string(errorString));
		}
	}
	else {
		numElements = Shader::GetUniformTypeElementCount(uniformType);

		if (numElements > 1 && (arrayLength % numElements) != 0) {
			snprintf(errorString,
				sizeof errorString,
				"Expected array length to be a multiple of %d, but it had %d elements instead!",
				numElements,
				arrayLength);

			throw ScriptException(std::string(errorString));
		}
	}

	int* valuesInt = nullptr;
	float* valuesFloat = nullptr;

	switch (uniformType) {
	case Shader::UNIFORM_FLOAT:
		*values = Memory::Malloc(sizeof(float) * arrayLength);
		valuesFloat = (float*)(*values);

		for (size_t i = 0; i < arrayLength; i++) {
			VMValue element = (*array->Values)[i];
			VMValue result = Value::CastAsDecimal(element);

			if (IS_NULL(result)) {
				Memory::Free(*values);

				ThrowElementTypeMismatchError(
					i, GetTypeString(VAL_DECIMAL), GetValueTypeString(element));
			}

			valuesFloat[i] = AS_DECIMAL(result);
		}
		break;
	case Shader::UNIFORM_INT:
		*values = Memory::Malloc(sizeof(int) * arrayLength);
		valuesInt = (int*)(*values);

		for (size_t i = 0; i < arrayLength; i++) {
			VMValue element = (*array->Values)[i];
			VMValue result = Value::CastAsInteger(element);

			if (IS_NULL(result)) {
				Memory::Free(*values);

				ThrowElementTypeMismatchError(
					i, GetTypeString(VAL_INTEGER), GetValueTypeString(element));
			}

			valuesInt[i] = AS_INTEGER(result);
		}
		break;
	case Shader::UNIFORM_FLOAT_VEC2:
	case Shader::UNIFORM_FLOAT_VEC3:
	case Shader::UNIFORM_FLOAT_VEC4:
		UNIMPLEMENTED_EXCEPTION(ScriptException);
	case Shader::UNIFORM_INT_VEC2:
	case Shader::UNIFORM_INT_VEC3:
	case Shader::UNIFORM_INT_VEC4:
	case Shader::UNIFORM_BOOL_VEC2:
	case Shader::UNIFORM_BOOL_VEC3:
	case Shader::UNIFORM_BOOL_VEC4:
		UNIMPLEMENTED_EXCEPTION(ScriptException);
	case Shader::UNIFORM_FLOAT_MAT2:
	case Shader::UNIFORM_FLOAT_MAT3:
	case Shader::UNIFORM_FLOAT_MAT4:
		UNIMPLEMENTED_EXCEPTION(ScriptException);
	default:
		UNREACHABLE_EXCEPTION(ScriptException);
	}

	return arrayLength / numElements;
}
void ThrowElementTypeMismatchError(size_t element, const char* expected, const char* elementType) {
	char errorString[256];

	snprintf(errorString,
		sizeof errorString,
		"Uniform type mismatch in array! Expected value at index %d to be of type %s; value was of type %s.",
		element,
		expected,
		elementType);

	throw ScriptException(std::string(errorString));
}
/***
 * \method SetTexture
 * \desc Assigns an Image to a specific texture unit linked to an uniform variable. The texture unit must have been defined using <linkto ref="shader.AssignTextureUnit"></linkto> for this function to succeed.
 * \param uniform (String): The name of the uniform.
 * \param image (Image): The image to bind to the uniform.
 * \ns Shader
 */
VMValue ShaderImpl::VM_SetTexture(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjShader* objShader = AS_SHADER(args[0]);
	Shader* shader = (Shader*)objShader->ShaderPtr;
	char* uniform = GET_ARG(1, GetString);
	Texture* texture = nullptr;

	CHECK_EXISTS(shader);

	if (!shader->WasCompiled()) {
		throw ScriptException("Cannot set texture before compilation!");
	}

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
 * \desc Deletes a shader. It can no longer be used after this function is called.
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
