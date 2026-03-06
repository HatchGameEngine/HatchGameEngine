#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Exceptions/CompilerErrorException.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/Utilities/StringUtils.h>

hsl_Compiler* hsl_compiler_new(hsl_Context* context) {
	if (!context) {
		return nullptr;
	}

	Compiler* compiler = new Compiler((ScriptManager*)context);
	compiler->InREPL = false;
	compiler->CurrentSettings = Compiler::Settings;
	compiler->Type = FUNCTIONTYPE_TOPLEVEL;
	compiler->ScopeDepth = 0;
	compiler->Initialize();
	compiler->SetupLocals();

	return (hsl_Compiler*)compiler;
}

hsl_Result hsl_compiler_set_settings(hsl_Compiler* compiler, hsl_CompilerSettings* settings) {
	if (!compiler) {
		return HSL_INVALID_ARGUMENT;
	}

	CompilerSettings currentSettings = Compiler::Settings;
	if (settings) {
		currentSettings.ShowWarnings = settings->show_warnings;
		currentSettings.WriteDebugInfo = settings->write_debug_info;
		currentSettings.WriteSourceFilename = settings->write_source_filename;
		currentSettings.DoOptimizations = settings->do_optimizations;
	}

	Compiler* compilerPtr = (Compiler*)compiler;
	compilerPtr->CurrentSettings = currentSettings;

	return HSL_OK;
}

hsl_Result hsl_compiler_free(hsl_Compiler* compiler) {
	if (!compiler) {
		return HSL_INVALID_ARGUMENT;
	}

	Compiler* compilerPtr = (Compiler*)compiler;
	delete compilerPtr;

	return HSL_OK;
}

hsl_Result hsl_compile_internal(Compiler* compiler, const char* code, MemoryStream** stream, const char* input_filename) {
	if (!compiler || !code || !stream) {
		return HSL_INVALID_ARGUMENT;
	}

	MemoryStream* memStream = MemoryStream::New(0x100);
	if (!memStream) {
		return HSL_OUT_OF_MEMORY;
	}

	Compiler::PrepareCompiling();

	if (compiler->Manager->LastCompileError) {
		Memory::Free(compiler->Manager->LastCompileError);
		compiler->Manager->LastCompileError = nullptr;
	}

	bool didCompile = false;
	try {
		didCompile = compiler->Compile(input_filename, code, memStream);
	} catch (const CompilerErrorException& error) {
		compiler->Manager->LastCompileError = StringUtils::Duplicate(error.what());
	}

	Compiler::FinishCompiling();

	if (!didCompile) {
		return HSL_COMPILE_ERROR;
	}

	*stream = memStream;

	return HSL_OK;
}

hsl_Result hsl_compile(hsl_Compiler* compiler, const char* code, char** out_bytecode, size_t *out_size, const char* input_filename) {
	MemoryStream* memStream = nullptr;

	hsl_Result result = hsl_compile_internal((Compiler*)compiler, code, &memStream, input_filename);
	if (result != HSL_OK) {
		return result;
	}

	*out_size = memStream->Position();
	*out_bytecode = (char*)malloc(*out_size);

	memStream->Seek(0);
	memStream->ReadBytes(*out_bytecode, *out_size);
	memStream->Close();

	return HSL_OK;
}
#endif
