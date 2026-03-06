#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/ScriptManager.h>

hsl_Module* hsl_load_script(hsl_Context* context, const char* code, hsl_CompilerSettings* settings, const char* input_filename) {
	MemoryStream* memStream = nullptr;
	ObjModule* loadedModule = nullptr;
	hsl_Compiler* compiler = hsl_compiler_new(context);
	if (!compiler) {
		return nullptr;
	}

	if (settings) {
		hsl_compiler_set_settings(compiler, settings);
	}

	Compiler* compilerPtr = (Compiler*)compiler;
	hsl_Result result = hsl_compile_internal(compilerPtr, code, &memStream, input_filename);
	if (result != HSL_OK) {
		hsl_compiler_free(compiler);
		return nullptr;
	}

	Uint32 filenameHash = 0xABCDABCD;
	if (input_filename) {
		filenameHash = ScriptManager::MakeFilenameHash(input_filename);
	}

	memStream->Seek(0);
	loadedModule = compilerPtr->Manager->LoadBytecode(memStream, filenameHash);
	memStream->Close();

	hsl_compiler_free(compiler);

	return (hsl_Module*)loadedModule;
}

hsl_Module* hsl_load_bytecode(hsl_Context* context, char* code, size_t size, const char* input_filename) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !code || !size) {
		return nullptr;
	}

	Uint32 filenameHash = 0xABCDABCD;
	if (input_filename) {
		filenameHash = ScriptManager::MakeFilenameHash(input_filename);
	}

	MemoryStream* memStream = MemoryStream::New(code, size);
	if (!memStream) {
		return nullptr;
	}

	ObjModule* loadedModule = manager->LoadBytecode(memStream, filenameHash);
	memStream->Close();

	return (hsl_Module*)loadedModule;
}

const char* hsl_get_compile_error(hsl_Context* context) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return nullptr;
	}

	return manager->LastCompileError;
}

hsl_Result hsl_run_module(hsl_Thread* thread, hsl_Module* module) {
	if (!thread || !module) {
		return HSL_INVALID_ARGUMENT;
	}

	hsl_Result result = hsl_setup_call_frame(thread, hsl_module_get_main_function(module), 0);
	if (result == HSL_OK) {
		return hsl_run_call_frame(thread);
	}

	return result;
}
#endif
