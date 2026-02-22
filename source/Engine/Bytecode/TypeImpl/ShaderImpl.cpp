#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/ShaderImpl.h>
#include <Engine/Bytecode/TypeImpl/StreamImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Graphics.h>
#include <Engine/Rendering/Shader.h>

/***
* \class Shader
* \desc Representation of a GPU shader.<br/>\
Not all devices will support shaders. Use `Device.GetCapability("graphics_shaders")` to verify if that's the case.<br/>\
The software renderer does not support shaders.
*/

size_t GetArrayUniformValues(ObjArray* array, void** values, Uint8 dataType);
bool IsArrayOfMatrices(ObjArray* array, size_t arrayLength, size_t expectedCount);
void ReadFloatVectorArray(ObjArray* array, size_t arrIndex, size_t numElements, void* values);
void ReadIntVectorArray(ObjArray* array, size_t arrIndex, size_t numElements, void* values);
void ThrowElementTypeMismatchError(size_t element, const char* expected, const char* elementType);

ObjClass* ShaderImpl::Class = nullptr;

static Uint32 Hash_Uniforms = 0;

void ShaderImpl::Init() {
	Class = NewClass(CLASS_SHADER);
	Class->NewFn = New;

	Hash_Uniforms = Murmur::EncryptString("Uniforms");

	ScriptManager::DefineNative(Class, "HasStage", VM_HasStage);
	ScriptManager::DefineNative(Class, "CanCompile", VM_CanCompile);
	ScriptManager::DefineNative(Class, "IsValid", VM_IsValid);
	ScriptManager::DefineNative(Class, "AddStage", VM_AddStage);
	ScriptManager::DefineNative(Class, "AddStageFromString", VM_AddStageFromString);
	ScriptManager::DefineNative(Class, "AssignTextureUnit", VM_AssignTextureUnit);
	ScriptManager::DefineNative(Class, "GetTextureUnit", VM_GetTextureUnit);
	ScriptManager::DefineNative(Class, "Compile", VM_Compile);
	ScriptManager::DefineNative(Class, "HasUniform", VM_HasUniform);
	ScriptManager::DefineNative(Class, "GetUniformType", VM_GetUniformType);
	ScriptManager::DefineNative(Class, "SetUniform", VM_SetUniform);
	ScriptManager::DefineNative(Class, "SetTexture", VM_SetTexture);
	ScriptManager::DefineNative(Class, "Delete", VM_Delete);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(CLASS_SHADER, Class);
	TypeImpl::DefinePrintableName(Class, "shader");
}

#define CHECK_EXISTS(ptr) \
	if (ptr == nullptr) { \
		throw ScriptException("Shader has been deleted!"); \
	}

bool ShaderImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	ObjShader* objShader = (ObjShader*)object;
	if (objShader == nullptr) {
		return false;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	CHECK_EXISTS(shader);

	if (hash == Hash_Uniforms) {
		if (!shader->WasCompiled()) {
			throw ScriptException("Cannot access this field before compilation!");
		}

		if (ScriptManager::Lock()) {
			ObjArray* array = NewArray();

			for (auto it = shader->UniformMap.begin(); it != shader->UniformMap.end();
				it++) {
				ObjString* name = CopyString(it->first.c_str());
				array->Values->push_back(OBJECT_VAL(name));
			}

			*result = OBJECT_VAL(array);
			ScriptManager::Unlock();
			return true;
		}
	}

	return false;
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

/***
 * \constructor
 * \desc Creates a shader program.
 * \ns Shader
 */
Obj* ShaderImpl::New() {
	Shader* shader = Graphics::CreateShader();
	if (shader == nullptr) {
		throw ScriptException("Could not create shader!");
	}

	ObjShader* obj = New((void*)shader);
	shader->Object = (void*)obj;
	return (Obj*)obj;
}
ObjShader* ShaderImpl::New(void* shaderPtr) {
	ObjShader* shader = (ObjShader*)NewNativeInstance(sizeof(ObjShader));
	Memory::Track(shader, "NewShader");
	shader->Object.Class = Class;
	shader->Object.PropertyGet = VM_PropertyGet;
	shader->Object.Destructor = Dispose;
	shader->ShaderPtr = shaderPtr;
	return shader;
}
void ShaderImpl::Dispose(Obj* object) {
	ObjShader* objShader = (ObjShader*)object;

	// Yes, this leaks memory.
	// Use Delete() in your script for a shader you no longer need!
	Shader* shader = (Shader*)objShader->ShaderPtr;
	if (shader != nullptr) {
		shader->Object = nullptr;
	}

	InstanceImpl::Dispose(object);
}
/***
 * \method HasStage
 * \desc Checks if the shader program has a shader stage.
 * \param stage (<ref SHADERSTAGE_*>): The shader stage.
 * \return boolean Returns whether there is a shader stage of the given type.
 * \ns Shader
 */
VMValue ShaderImpl::VM_HasStage(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjShader* objShader = GET_ARG(0, GetShader);
	if (objShader == nullptr) {
		return NULL_VAL;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	int stage = GET_ARG(1, GetInteger);

	CHECK_EXISTS(shader);

	bool hasStage = shader->HasStage(stage);

	return INTEGER_VAL(hasStage);
}
/***
 * \method CanCompile
 * \desc Checks if the shader can be compiled.
 * \return boolean Returns whether the shader can be compiled.
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

	bool canCompile = shader->CanCompile();

	return INTEGER_VAL(canCompile);
}
/***
 * \method IsValid
 * \desc Checks if the shader can be used.
 * \return boolean Returns whether the shader can be used.
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

	bool isValid = shader->IsValid();

	return INTEGER_VAL(isValid);
}
/***
 * \method AddStage
 * \desc Adds a stage to the shader program. This should not be called multiple times for the same stage.
 * \param stage (<ref SHADERSTAGE_*>): The shader stage.
 * \param shader (string): Filename of the resource containing shader code.
 * \ns Shader
 */
/***
 * \method AddStage
 * \desc Adds a stage to the shader program. This should not be called multiple times for the same stage.
 * \param stage (<ref SHADERSTAGE_*>): The shader stage.
 * \param shader (stream): A stream containing shader code.
 * \ns Shader
 */
VMValue ShaderImpl::VM_AddStage(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjShader* objShader = GET_ARG(0, GetShader);
	if (objShader == nullptr) {
		return NULL_VAL;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	int stage = GET_ARG(1, GetInteger);
	Stream* stream = nullptr;
	bool closeStream = false;

	CHECK_EXISTS(shader);

	if (IS_STREAM(args[2])) {
		ObjStream* objStream = AS_STREAM(args[2]);
		if (!objStream || !objStream->StreamPtr) {
			return NULL_VAL;
		}

		if (objStream->Closed) {
			throw ScriptException("Cannot read closed stream!");
		}
		if (objStream->Writable) {
			throw ScriptException("Stream is not readable!");
		}

		stream = objStream->StreamPtr;
	}
	else {
		char* filename = GET_ARG(2, GetString);

		stream = ResourceStream::New(filename);

		if (!stream) {
			throw ScriptException(
				"Resource \"" + std::string(filename) + "\" does not exist!");
		}

		closeStream = true;
	}

	bool success = false;

	try {
		shader->AddStage(stage, stream);

		success = true;
	} catch (const std::runtime_error& error) {
		std::string errorMessage = "Error adding shader stage: ";

		errorMessage += std::string(error.what());

		ScriptManager::Threads[threadID].ShowErrorLocation(errorMessage.c_str());
	}

	if (closeStream) {
		stream->Close();
	}

	return INTEGER_VAL(success);
}
/***
 * \method AddStageFromString
 * \desc Adds a stage to the shader program, from a String containing the shader code. This should not be called multiple times for the same stage.
 * \param stage (<ref SHADERSTAGE_*>): The shader stage.
 * \param shader (string): A String that contains shader code.
 * \ns Shader
 */
VMValue ShaderImpl::VM_AddStageFromString(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjShader* objShader = GET_ARG(0, GetShader);
	if (objShader == nullptr) {
		return NULL_VAL;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	int stage = GET_ARG(1, GetInteger);
	char* shaderCode = GET_ARG(2, GetString);

	CHECK_EXISTS(shader);

	TextStream* stream = TextStream::New(shaderCode);

	bool success = false;

	try {
		shader->AddStage(stage, stream);

		success = true;
	} catch (const std::runtime_error& error) {
		std::string errorMessage = "Error adding shader stage: ";

		errorMessage += std::string(error.what());

		ScriptManager::Threads[threadID].ShowErrorLocation(errorMessage.c_str());
	}

	stream->Close();

	return INTEGER_VAL(success);
}
/***
 * \method AssignTextureUnit
 * \desc Assigns a texture unit to a texture uniform. This is safe to call multiple times for the same uniform name.
 * \param uniform (string): The name of the uniform.
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
 * \param uniform (string): The name of the uniform.
 * \return integer Returns an integer value, or `null` if no texture unit was assigned.
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
 * \return boolean Returns whether the shader was compiled.
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
 * \param uniform (string): The name of the uniform.
 * \return boolean Returns whether there is an uniform with the given name.
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
 * \method GetUniformType
 * \desc Gets the type of the uniform with the given name. This can only be called after the shader has been compiled.
 * \param uniform (string): The name of the uniform.
 * \return <ref SHADERSTAGE_*> Returns the data type of the uniform, or `null` if there is no uniform with that name.
 * \ns Shader
 */
VMValue ShaderImpl::VM_GetUniformType(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjShader* objShader = AS_SHADER(args[0]);
	Shader* shader = (Shader*)objShader->ShaderPtr;
	char* uniformName = GET_ARG(1, GetString);

	CHECK_EXISTS(shader);

	if (!shader->WasCompiled()) {
		throw ScriptException("Cannot get uniform type before compilation!");
	}

	ShaderUniform* uniform = shader->GetUniform(uniformName);
	if (uniform == nullptr) {
		throw ScriptException("No uniform named \"" + std::string(uniformName) + "\"!");
	}

	return INTEGER_VAL(uniform->Type);
}
/***
 * \method SetUniform
 * \desc Sets the value of an uniform variable. The value passed to the uniform persists between different invocations, and can be set at any time, as long as the shader is valid. This can only be called after the shader has been compiled.
 * \param uniform (string): The name of the uniform.
 * \param value (number): The value.
 * \paramOpt dataType (<ref SHADERDATATYPE_*>): The data type of the value being sent. This is required if the value being passed is an array.
 * \ns Shader
 */
/***
 * \method SetUniform
 * \desc Sets the value of an uniform variable. The value passed to the uniform persists between different invocations, and can be set at any time, as long as the shader is valid. This can only be called after the shader has been compiled.
 * \param uniform (string): The name of the uniform.
 * \param value (array): The value.
 * \paramOpt dataType (<ref SHADERDATATYPE_*>): The data type of the value being sent. This is required if the value being passed is an array.
 * \ns Shader
 */
VMValue ShaderImpl::VM_SetUniform(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 3);

	ObjShader* objShader = AS_SHADER(args[0]);
	char* uniformName = GET_ARG(1, GetString);
	VMValue value = args[2];
	int dataType = Shader::DATATYPE_UNKNOWN;

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

	if (argCount >= 4) {
		int type = GET_ARG(3, GetInteger);
		if (type < Shader::DATATYPE_FLOAT || type > Shader::DATATYPE_SAMPLER_CUBE) {
			char errorString[64];

			snprintf(errorString,
				sizeof errorString,
				"Data type %d out of range. (0 - %d)",
				type,
				Shader::DATATYPE_SAMPLER_CUBE);

			throw ScriptException(std::string(errorString));
		}

		dataType = (Uint8)type;
	}

	if (Shader::DataTypeIsSampler(dataType)) {
		throw ScriptException(
			"Cannot change texture unit! Use AssignTextureUnit before compilation.");
	}

	// Check type of value
	bool isArrayType = IS_ARRAY(value);
	bool typeMismatch = false;

	Uint8 type;

	if (IS_INTEGER(value)) {
		// Lets you pass an int to an uniform that is expecting a float.
		if (uniform->Type == Shader::DATATYPE_FLOAT) {
			type = Shader::DATATYPE_FLOAT;
			value = Value::CastAsDecimal(value);
		}
		else {
			type = Shader::DATATYPE_INT;
		}

		if (dataType != Shader::DATATYPE_UNKNOWN && dataType != Shader::DATATYPE_INT) {
			if (dataType == Shader::DATATYPE_FLOAT) {
				value = Value::CastAsDecimal(value);
			}
			else {
				typeMismatch = true;
			}
		}
	}
	else if (IS_DECIMAL(value)) {
		type = Shader::DATATYPE_FLOAT;

		if (dataType != Shader::DATATYPE_UNKNOWN && dataType != Shader::DATATYPE_FLOAT) {
			if (dataType == Shader::DATATYPE_INT) {
				value = Value::CastAsInteger(value);
			}
			else {
				typeMismatch = true;
			}
		}
	}
	else if (isArrayType) {
		if (dataType == Shader::DATATYPE_UNKNOWN) {
			throw ScriptException("Must specify uniform type if value is an array!");
		}

		switch (dataType) {
		case Shader::DATATYPE_FLOAT:
		case Shader::DATATYPE_FLOAT_VEC2:
		case Shader::DATATYPE_FLOAT_VEC3:
		case Shader::DATATYPE_FLOAT_VEC4:
		case Shader::DATATYPE_INT:
		case Shader::DATATYPE_INT_VEC2:
		case Shader::DATATYPE_INT_VEC3:
		case Shader::DATATYPE_INT_VEC4:
		case Shader::DATATYPE_BOOL:
		case Shader::DATATYPE_BOOL_VEC2:
		case Shader::DATATYPE_BOOL_VEC3:
		case Shader::DATATYPE_BOOL_VEC4:
		case Shader::DATATYPE_FLOAT_MAT2:
		case Shader::DATATYPE_FLOAT_MAT3:
		case Shader::DATATYPE_FLOAT_MAT4:
			// Okay.
			type = dataType;
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
		count = GetArrayUniformValues(AS_ARRAY(value), &values, dataType);
	}
	else if (type == Shader::DATATYPE_INT) {
		values = Memory::Malloc(sizeof(int) * count);

		int* valuesInt = (int*)values;
		valuesInt[0] = AS_INTEGER(value);
	}
	else if (type == Shader::DATATYPE_FLOAT) {
		values = Memory::Malloc(sizeof(float) * count);

		float* valuesFloat = (float*)values;
		valuesFloat[0] = AS_DECIMAL(value);
	}

	if (values == nullptr || count == 0) {
		throw ScriptException("Could not allocate memory for sending data to shader!");
	}

	try {
		shader->SetUniform(uniform, count, values, type);
	} catch (const std::runtime_error& error) {
		throw ScriptException(error.what());
	}

	Memory::Free(values);

	return NULL_VAL;
}
size_t GetArrayUniformValues(ObjArray* array, void** values, Uint8 dataType) {
	size_t arrayLength = array->Values->size();
	size_t count = arrayLength;
	size_t numElements = 1;

	bool isArrayOfMatrices = false;

	char errorString[128];

	if (Shader::DataTypeIsMatrix(dataType)) {
		numElements = Shader::GetMatrixDataTypeSize(dataType);
		isArrayOfMatrices = IsArrayOfMatrices(array, arrayLength, numElements);

		if (!isArrayOfMatrices) {
			count = 1;

			if (arrayLength != numElements) {
				snprintf(errorString,
					sizeof errorString,
					"Expected array to have exactly %zu elements, but it had %zu elements instead!",
					numElements,
					arrayLength);

				throw ScriptException(std::string(errorString));
			}
		}
	}
	else {
		numElements = Shader::GetDataTypeElementCount(dataType);

		if (numElements > 1 && (arrayLength % numElements) != 0) {
			snprintf(errorString,
				sizeof errorString,
				"Expected array length to be a multiple of %zu, but it had %zu elements instead!",
				numElements,
				arrayLength);

			throw ScriptException(std::string(errorString));
		}
	}

	// Allocate the memory
	size_t totalDataSize = arrayLength * numElements;

	switch (dataType) {
	case Shader::DATATYPE_FLOAT:
	case Shader::DATATYPE_FLOAT_MAT2:
	case Shader::DATATYPE_FLOAT_MAT3:
	case Shader::DATATYPE_FLOAT_MAT4:
	case Shader::DATATYPE_FLOAT_VEC2:
	case Shader::DATATYPE_FLOAT_VEC3:
	case Shader::DATATYPE_FLOAT_VEC4:
		totalDataSize *= sizeof(float);
		break;
	case Shader::DATATYPE_INT:
	case Shader::DATATYPE_INT_VEC2:
	case Shader::DATATYPE_INT_VEC3:
	case Shader::DATATYPE_INT_VEC4:
	case Shader::DATATYPE_BOOL_VEC2:
	case Shader::DATATYPE_BOOL_VEC3:
	case Shader::DATATYPE_BOOL_VEC4:
		totalDataSize *= sizeof(int);
		break;
	}

	*values = Memory::Malloc(totalDataSize);
	if (*values == nullptr) {
		return 0;
	}

	// Read the values
	switch (dataType) {
	case Shader::DATATYPE_FLOAT_MAT2:
	case Shader::DATATYPE_FLOAT_MAT3:
	case Shader::DATATYPE_FLOAT_MAT4:
		if (isArrayOfMatrices) {
			float* buf = (float*)(*values);
			for (size_t i = 0; i < arrayLength; i++) {
				ObjArray* matArray = AS_ARRAY((*array->Values)[i]);

				// Assumed to be of the correct size (because it was validated.)
				for (size_t j = 0; j < numElements; j++) {
					VMValue result =
						Value::CastAsDecimal((*matArray->Values)[j]);
					*buf++ = AS_DECIMAL(result);
				}
			}
			break;
		}
		// Intentional fallthrough.
	case Shader::DATATYPE_FLOAT:
		for (size_t i = 0; i < arrayLength; i++) {
			VMValue element = (*array->Values)[i];
			VMValue result = Value::CastAsDecimal(element);

			if (IS_NULL(result)) {
				Memory::Free(*values);

				ThrowElementTypeMismatchError(
					i, GetTypeString(VAL_DECIMAL), GetValueTypeString(element));
			}

			((float*)(*values))[i] = AS_DECIMAL(result);
		}
		break;
	case Shader::DATATYPE_INT:
		for (size_t i = 0; i < arrayLength; i++) {
			VMValue element = (*array->Values)[i];
			VMValue result = Value::CastAsInteger(element);

			if (IS_NULL(result)) {
				Memory::Free(*values);

				ThrowElementTypeMismatchError(
					i, GetTypeString(VAL_INTEGER), GetValueTypeString(element));
			}

			((int*)(*values))[i] = AS_INTEGER(result);
		}
		break;
	case Shader::DATATYPE_FLOAT_VEC2:
	case Shader::DATATYPE_FLOAT_VEC3:
	case Shader::DATATYPE_FLOAT_VEC4:
		for (size_t i = 0; i < arrayLength; i++) {
			ReadFloatVectorArray(array, i, numElements, *values);
		}
		break;
	case Shader::DATATYPE_INT_VEC2:
	case Shader::DATATYPE_INT_VEC3:
	case Shader::DATATYPE_INT_VEC4:
	case Shader::DATATYPE_BOOL_VEC2:
	case Shader::DATATYPE_BOOL_VEC3:
	case Shader::DATATYPE_BOOL_VEC4:
		for (size_t i = 0; i < arrayLength; i++) {
			ReadIntVectorArray(array, i, numElements, *values);
		}
		break;
	default:
		return 0;
	}

	return count;
}
bool IsArrayOfMatrices(ObjArray* array, size_t arrayLength, size_t expectedCount) {
	if (arrayLength == 0) {
		return false;
	}

	for (size_t i = 0; i < arrayLength; i++) {
		VMValue element = (*array->Values)[i];

		if (!IS_ARRAY(element)) {
			// Not an array of arrays
			return false;
		}
	}

	// They are all arrays, so continue validations.
	for (size_t i = 0; i < arrayLength; i++) {
		ObjArray* matArray = AS_ARRAY((*array->Values)[i]);
		size_t matArraySize = matArray->Values->size();

		if (matArraySize != expectedCount) {
			char errorString[128];

			snprintf(errorString,
				sizeof errorString,
				"Expected matrix at array index %zu to have exactly %zu elements, but it had %zu elements instead!",
				i,
				expectedCount,
				matArraySize);

			throw ScriptException(std::string(errorString));
		}

		for (size_t j = 0; j < matArraySize; j++) {
			VMValue result = Value::CastAsDecimal((*matArray->Values)[j]);

			if (IS_NULL(result)) {
				return false;
			}
		}
	}

	return true;
}
void ReadFloatVectorArray(ObjArray* array, size_t arrIndex, size_t numElements, void* values) {
	char errorString[128];

	VMValue element = (*array->Values)[arrIndex];
	if (!IS_ARRAY(element)) {
		Memory::Free(values);

		ThrowElementTypeMismatchError(
			arrIndex, GetObjectTypeString(OBJ_ARRAY), GetValueTypeString(element));
	}

	ObjArray* vecArray = AS_ARRAY(element);
	size_t vecArraySize = vecArray->Values->size();
	if (vecArraySize != numElements) {
		snprintf(errorString,
			sizeof errorString,
			"Expected vector array to have exactly %zu elements, but it had %zu elements instead!",
			numElements,
			vecArraySize);

		Memory::Free(values);

		throw ScriptException(std::string(errorString));
	}

	float* valuesFloat = (float*)values;
	size_t offset = arrIndex * numElements;

	for (size_t i = 0; i < numElements; i++) {
		VMValue vecElement = (*vecArray->Values)[i];
		VMValue result = Value::CastAsDecimal(vecElement);

		if (IS_NULL(result)) {
			Memory::Free(values);

			snprintf(errorString,
				sizeof errorString,
				"Uniform type mismatch in vector array at index %zu! Expected value at index %zu to be of type %s; value was of type %s.",
				arrIndex,
				i,
				GetTypeString(VAL_INTEGER),
				GetValueTypeString(vecElement));

			throw ScriptException(std::string(errorString));
		}

		valuesFloat[offset + i] = AS_DECIMAL(result);
	}
}
void ReadIntVectorArray(ObjArray* array, size_t arrIndex, size_t numElements, void* values) {
	char errorString[128];

	VMValue element = (*array->Values)[arrIndex];
	if (!IS_ARRAY(element)) {
		Memory::Free(values);

		ThrowElementTypeMismatchError(
			arrIndex, GetObjectTypeString(OBJ_ARRAY), GetValueTypeString(element));
	}

	ObjArray* vecArray = AS_ARRAY(element);
	size_t vecArraySize = vecArray->Values->size();
	if (vecArraySize != numElements) {
		snprintf(errorString,
			sizeof errorString,
			"Expected vector array to have exactly %zu elements, but it had %zu elements instead!",
			numElements,
			vecArraySize);

		Memory::Free(values);

		throw ScriptException(std::string(errorString));
	}

	int* valuesInt = (int*)values;
	size_t offset = arrIndex * numElements;

	for (size_t i = 0; i < numElements; i++) {
		VMValue vecElement = (*vecArray->Values)[i];
		VMValue result = Value::CastAsInteger(vecElement);

		if (IS_NULL(result)) {
			Memory::Free(values);

			snprintf(errorString,
				sizeof errorString,
				"Uniform type mismatch in vector array at index %zu! Expected value at index %zu to be of type %s; value was of type %s.",
				arrIndex,
				i,
				GetTypeString(VAL_INTEGER),
				GetValueTypeString(vecElement));

			throw ScriptException(std::string(errorString));
		}

		valuesInt[offset + i] = AS_INTEGER(result);
	}
}
void ThrowElementTypeMismatchError(size_t element, const char* expected, const char* elementType) {
	char errorString[256];

	snprintf(errorString,
		sizeof errorString,
		"Uniform type mismatch in array! Expected value at index %zu to be of type %s; value was of type %s.",
		element,
		expected,
		elementType);

	throw ScriptException(std::string(errorString));
}
/***
 * \method SetTexture
 * \desc Assigns an Image to a specific texture unit linked to an uniform variable. The texture unit must have been defined using <ref Shader.AssignTextureUnit> for this function to succeed.
 * \param uniform (string): The name of the uniform.
 * \param image (integer): The image index to bind to the uniform.
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
