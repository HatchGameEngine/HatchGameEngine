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

	ScriptManager::DefineNative(Class, "AddProgram", VM_AddProgram);
	ScriptManager::DefineNative(Class, "Compile", VM_Compile);
	ScriptManager::DefineNative(Class, "Delete", VM_Delete);

	ScriptManager::ClassImplList.push_back(Class);

	ScriptManager::Globals->Put(className, OBJECT_VAL(Class));
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

#define CHECK_VALID(ptr) \
	if (ptr == nullptr) { \
		ScriptManager::Threads[threadID].ThrowRuntimeError( \
			false, "Shader is no longer valid!"); \
		return NULL_VAL; \
	}

/***
 * \constructor
 * \desc Creates a shader.
 * \ns Shader
 */
Obj* ShaderImpl::VM_New() {
	Shader* shader = Graphics::CreateShader();
	if (shader == nullptr) {
		throw std::runtime_error("Could not create shader!");
	}

	return (Obj*)NewShader((void*)shader);
}
/***
 * \method AddProgram
 * \desc Adds a program to a shader.
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
	CHECK_VALID(shader);

	int program = GET_ARG(1, GetInteger);
	char* filename = GET_ARG(2, GetString);

	Stream* stream = ResourceStream::New(filename);
	if (!stream) {
		throw std::runtime_error("Resource \"" + std::string(filename) + "\" does not exist!");
	}

	try {
		shader->AddProgram(program, stream);
	} catch (const std::runtime_error& error) {
		stream->Close();

		// Your problem now buddy.
		throw;
	}

	stream->Close();

	return NULL_VAL;
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

	CHECK_VALID(shader);

	shader->Compile();

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

	CHECK_VALID(shader);

	Graphics::DeleteShader(shader);

	objShader->ShaderPtr = nullptr;

	return NULL_VAL;
}

#undef CHECK_VALID
#undef GET_ARG
