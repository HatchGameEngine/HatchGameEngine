#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandaloneMain.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Exceptions/CompilerErrorException.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/Utilities/StringUtils.h>

const char* hsl_get_version() {
	return GetVersionText();
}

hsl_Result hsl_init() {
	InitSubsystems();

	return HSL_OK;
}

void hsl_finish() {
	DisposeSubsystems();
}

hsl_Context* hsl_new_context() {
	ScriptManager* manager = new ScriptManager;
	return (hsl_Context*)manager;
}

hsl_Result hsl_free_context(hsl_Context* context) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return HSL_INVALID_ARGUMENT;
	}

	delete manager;

	return HSL_OK;
}

hsl_Result hsl_set_scripts_directory(const char* directory) {
	SetScriptsDirectory(directory);

	return HSL_OK;
}

hsl_Result hsl_set_import_script_handler(hsl_Context* context, hsl_ImportScriptHandler handler) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return HSL_INVALID_ARGUMENT;
	}

	manager->SetImportScriptHandler(handler);

	return HSL_OK;
}

hsl_Result hsl_set_import_class_handler(hsl_Context* context, hsl_ImportClassHandler handler) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return HSL_INVALID_ARGUMENT;
	}

	manager->SetImportClassHandler(handler);

	return HSL_OK;
}

hsl_Result hsl_set_with_iterator_handler(hsl_Context* context, hsl_WithIteratorHandler handler) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return HSL_INVALID_ARGUMENT;
	}

	manager->SetWithIteratorHandler(handler);

	return HSL_OK;
}

hsl_Result hsl_set_runtime_error_handler(hsl_Context* context, hsl_RuntimeErrorHandler handler) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return HSL_INVALID_ARGUMENT;
	}

	manager->RuntimeErrorHandler = handler;

	return HSL_OK;
}

hsl_Result hsl_set_log_callback(hsl_LogCallback callback) {
	Log::SetCallback(callback);

	return HSL_OK;
}

hsl_Thread* hsl_new_thread(hsl_Context* context) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return nullptr;
	}

	VMThread* vmThread = manager->NewThread();
	if (!vmThread) {
		return nullptr;
	}

	return (hsl_Thread*)vmThread;
}

void hsl_free_thread(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return;
	}

	vmThread->Manager->DisposeThread(vmThread);
}

hsl_Context* hsl_get_thread_context(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	return (hsl_Context*)vmThread->Manager;
}

hsl_Result hsl_compile_internal(ScriptManager* manager, const char* code, hsl_CompilerSettings* settings, MemoryStream** stream, const char* input_filename) {
	if (!manager || !code || !settings || !stream) {
		return HSL_INVALID_ARGUMENT;
	}

	MemoryStream* memStream = MemoryStream::New(0x100);
	if (!memStream) {
		return HSL_OUT_OF_MEMORY;
	}

	CompilerSettings currentSettings = Compiler::Settings;
	if (settings) {
		currentSettings.ShowWarnings = settings->show_warnings;
		currentSettings.WriteDebugInfo = settings->write_debug_info;
		currentSettings.WriteSourceFilename = settings->write_source_filename;
		currentSettings.DoOptimizations = settings->do_optimizations;
	}

	Compiler::PrepareCompiling();

	Compiler* compiler = new Compiler(manager);
	compiler->InREPL = false;
	compiler->CurrentSettings = currentSettings;
	compiler->Type = FUNCTIONTYPE_TOPLEVEL;
	compiler->ScopeDepth = 0;
	compiler->Initialize();
	compiler->SetupLocals();

	if (manager->LastCompileError) {
		Memory::Free(manager->LastCompileError);
		manager->LastCompileError = nullptr;
	}

	bool didCompile = false;
	try {
		didCompile = compiler->Compile(input_filename, code, memStream);
	} catch (const CompilerErrorException& error) {
		manager->LastCompileError = StringUtils::Duplicate(error.what());
	}

	delete compiler;
	Compiler::FinishCompiling();

	if (!didCompile) {
		return HSL_COMPILE_ERROR;
	}

	*stream = memStream;

	return HSL_OK;
}

hsl_Result hsl_compile(hsl_Context* context, const char* code, hsl_CompilerSettings* settings, char** out_bytecode, size_t *out_size, const char* input_filename) {
	MemoryStream* memStream = nullptr;

	hsl_Result result = hsl_compile_internal((ScriptManager*)context, code, settings, &memStream, input_filename);
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

hsl_Result hsl_load_script(const char* code, hsl_CompilerSettings* settings, hsl_Thread* thread, hsl_Module** module, const char* input_filename) {
	MemoryStream* memStream = nullptr;
	ObjModule* loadedModule = nullptr;
	VMThread* vmThread = (VMThread*)thread;

	hsl_Result result = hsl_compile_internal(vmThread->Manager, code, settings, &memStream, input_filename);
	if (result != HSL_OK) {
		return result;
	}

	Uint32 filenameHash = 0xABCDABCD;
	if (input_filename) {
		filenameHash = vmThread->Manager->MakeFilenameHash(input_filename);
	}

	memStream->Seek(0);
	loadedModule = vmThread->Manager->LoadBytecode((VMThread*)thread, memStream, filenameHash);
	memStream->Close();

	if (!loadedModule) {
		return HSL_BYTECODE_LOAD_ERROR;
	}

	*module = (hsl_Module*)loadedModule;

	return HSL_OK;
}

hsl_Result hsl_load_bytecode(char* code, size_t size, hsl_Thread* thread, hsl_Module** module, const char* input_filename) {
	MemoryStream* memStream = MemoryStream::New(code, size);
	ObjModule* loadedModule = nullptr;
	VMThread* vmThread = (VMThread*)thread;

	Uint32 filenameHash = 0xABCDABCD;
	if (input_filename) {
		filenameHash = vmThread->Manager->MakeFilenameHash(input_filename);
	}

	loadedModule = vmThread->Manager->LoadBytecode(vmThread, memStream, filenameHash);
	memStream->Close();

	if (!loadedModule) {
		return HSL_BYTECODE_LOAD_ERROR;
	}

	*module = (hsl_Module*)loadedModule;

	return HSL_OK;
}

const char* hsl_get_compile_error(hsl_Context* context) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return nullptr;
	}

	return manager->LastCompileError;
}

int hsl_get_module_function_count(hsl_Module* module) {
	ObjModule* objModule = (ObjModule*)module;
	size_t size = objModule->Functions->size();
	return (int)size;
}

hsl_Result hsl_get_module_function(hsl_Module* module, size_t index, hsl_Function** function) {
	ObjModule* objModule = (ObjModule*)module;
	ObjFunction* objFunction;

	if (index >= objModule->Functions->size()) {
		return HSL_INVALID_ARGUMENT;
	}

	objFunction = (*objModule->Functions)[index];
	*function = (hsl_Function*)objFunction;

	return HSL_OK;
}

hsl_Function* hsl_get_main_function(hsl_Module* module) {
	ObjModule* objModule = (ObjModule*)module;
	ObjFunction* objFunction = (*objModule->Functions)[0];
	return (hsl_Function*)objFunction;
}

hsl_Result hsl_setup_call_frame(hsl_Thread* thread, hsl_Function* function, int num_args) {
	VMThread* vmThread = (VMThread*)thread;
	ObjFunction* objFunction = (ObjFunction*)function;

	if (objFunction->MinArity < objFunction->Arity) {
		if (num_args < objFunction->MinArity) {
			return HSL_NOT_ENOUGH_ARGS;
		}
		else if (num_args > objFunction->Arity) {
			return HSL_ARG_COUNT_MISMATCH;
		}
	}
	else if (num_args != objFunction->Arity) {
		return HSL_ARG_COUNT_MISMATCH;
	}

	if (vmThread->FrameCount == FRAMES_MAX) {
		return HSL_CALL_FRAME_OVERFLOW;
	}

	if (!vmThread->Call(objFunction, num_args)) {
		return HSL_COULD_NOT_CALL;
	}

	vmThread->ReturnFrame = vmThread->FrameCount - 1;

	return HSL_OK;
}

hsl_Result hsl_run_call_frame(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (vmThread->FrameCount == 0) {
		return HSL_NO_CALL_FRAMES;
	}

	hsl_Result result = HSL_OK;

	while (true) {
		int ret = vmThread->RunInstruction();
		if (ret == INTERPRET_FINISHED) {
			break;
		}
		else if (ret != INTERPRET_OK) {
			result = HSL_RUNTIME_ERROR;
			break;
		}
#ifdef VM_DEBUG
		else if (vmThread->FrameCount == 0) {
			break;
		}
#endif
	}

	return result;
}

hsl_Result hsl_run_call_frame_instruction(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (vmThread->FrameCount == 0) {
		return HSL_NO_CALL_FRAMES;
	}

	int ret = vmThread->RunInstruction();
	if (ret != INTERPRET_OK && ret != INTERPRET_FINISHED) {
		return HSL_RUNTIME_ERROR;
	}

	return HSL_OK;
}
#endif
