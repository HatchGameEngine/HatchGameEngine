#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandaloneMain.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Exceptions/CompilerErrorException.h>
#include <Engine/IO/MemoryStream.h>

std::string LastCompileError;

void hsl_init() {
    InitSubsystems();
}

void hsl_finish() {
    DisposeSubsystems();
}

const char* hsl_get_version() {
    return GetVersionText();
}

const char* hsl_get_compile_error() {
    if (LastCompileError.size() > 0) {
        return LastCompileError.c_str();
    }

    return nullptr;
}

void hsl_set_scripts_directory(const char* directory) {
    SetScriptsDirectory(directory);
}

void hsl_set_import_script_handler(hsl_ImportScriptHandler handler) {
    ScriptManager::SetImportScriptHandler(handler);
}

void hsl_set_import_class_handler(hsl_ImportClassHandler handler) {
    ScriptManager::SetImportClassHandler(handler);
}

void hsl_set_with_iterator_handler(hsl_WithIteratorHandler handler) {
    ScriptManager::SetWithIteratorHandler(handler);
}

void hsl_set_log_callback(hsl_LogCallback callback) {
    Log::SetCallback(callback);
}

void hsl_set_runtime_error_handler(hsl_RuntimeErrorHandler handler) {
    SetVMRuntimeErrorHandler(handler);
}

hsl_Result hsl_new_thread(struct hsl_Thread** thread) {
    VMThread* vmThread = ScriptManager::NewThread();
    if (!vmThread) {
        return HSL_TOO_MANY_THREADS;
    }

    *thread = (struct hsl_Thread*)vmThread;

    return HSL_OK;
}

void hsl_free_thread(struct hsl_Thread* thread) {
    ScriptManager::DisposeThread((VMThread*)thread);
}

hsl_Result hsl_compile_internal(const char* code, struct hsl_CompilerSettings* settings, MemoryStream** stream, const char* input_filename) {
    Compiler::PrepareCompiling();

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

    Compiler* compiler = new Compiler;
    compiler->InREPL = false;
    compiler->CurrentSettings = currentSettings;
    compiler->Type = FUNCTIONTYPE_TOPLEVEL;
    compiler->ScopeDepth = 0;
    compiler->Initialize();
    compiler->SetupLocals();

    LastCompileError = "";

    bool didCompile = false;
    try {
        didCompile = compiler->Compile(input_filename, code, memStream);
    } catch (const CompilerErrorException& error) {
        LastCompileError = std::string(error.what());
    }

    delete compiler;
    Compiler::FinishCompiling();

    if (!didCompile) {
        return HSL_COMPILE_ERROR;
    }

    *stream = memStream;

    return HSL_OK;
}

hsl_Result hsl_compile(const char* code, struct hsl_CompilerSettings* settings, char** out_bytecode, size_t *out_size, const char* input_filename) {
    MemoryStream* memStream = nullptr;

    hsl_Result result = hsl_compile_internal(code, settings, &memStream, input_filename);
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

hsl_Result hsl_load_script(const char* code, struct hsl_CompilerSettings* settings, struct hsl_Thread* thread, struct hsl_Module** module, const char* input_filename) {
    MemoryStream* memStream = nullptr;
    ObjModule* loadedModule = nullptr;

    hsl_Result result = hsl_compile_internal(code, settings, &memStream, input_filename);
    if (result != HSL_OK) {
        return result;
    }

    Uint32 filenameHash = 0xABCDABCD;
    if (input_filename) {
        filenameHash = ScriptManager::MakeFilenameHash(input_filename);
    }

    memStream->Seek(0);
    loadedModule = ScriptManager::LoadBytecode((VMThread*)thread, memStream, filenameHash);
    memStream->Close();

    if (!loadedModule) {
        return HSL_BYTECODE_LOAD_ERROR;
    }

    *module = (struct hsl_Module*)loadedModule;

    return HSL_OK;
}

hsl_Result hsl_load_bytecode(char* code, size_t size, struct hsl_Thread* thread, struct hsl_Module** module, const char* input_filename) {
    MemoryStream* memStream = MemoryStream::New(code, size);
    ObjModule* loadedModule = nullptr;

    Uint32 filenameHash = 0xABCDABCD;
    if (input_filename) {
        filenameHash = ScriptManager::MakeFilenameHash(input_filename);
    }

    loadedModule = ScriptManager::LoadBytecode((VMThread*)thread, memStream, filenameHash);
    memStream->Close();

    if (!loadedModule) {
        return HSL_BYTECODE_LOAD_ERROR;
    }

    *module = (struct hsl_Module*)loadedModule;

    return HSL_OK;
}

int hsl_get_module_function_count(struct hsl_Module* module) {
    ObjModule* objModule = (ObjModule*)module;
    size_t size = objModule->Functions->size();
    return (int)size;
}

hsl_Result hsl_get_module_function(struct hsl_Module* module, size_t index, struct hsl_Function** function) {
    ObjModule* objModule = (ObjModule*)module;
    ObjFunction* objFunction;

    if (index >= objModule->Functions->size()) {
        return HSL_INVALID_ARGUMENT;
    }

    objFunction = (*objModule->Functions)[index];
    *function = (struct hsl_Function*)objFunction;

    return HSL_OK;
}

struct hsl_Function* hsl_get_main_function(struct hsl_Module* module) {
    ObjModule* objModule = (ObjModule*)module;
    ObjFunction* objFunction = (*objModule->Functions)[0];
    return (struct hsl_Function*)objFunction;
}

hsl_Result hsl_setup_call_frame(struct hsl_Thread* thread, struct hsl_Function* function, int num_args) {
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

    return HSL_OK;
}

hsl_Result hsl_run_call_frame(struct hsl_Thread* thread) {
    VMThread* vmThread = (VMThread*)thread;
    if (vmThread->FrameCount == 0) {
        return HSL_NO_CALL_FRAMES;
    }

    while (true) {
        int ret = vmThread->RunInstruction();
        if (ret == INTERPRET_FINISHED) {
            break;
        }
        else if (ret != INTERPRET_OK) {
            return HSL_RUNTIME_ERROR;
        }
#ifdef VM_DEBUG
        else if (vmThread->FrameCount == 0) {
            break;
        }
#endif
    }

    return HSL_OK;
}

hsl_Result hsl_run_call_frame_instruction(struct hsl_Thread* thread) {
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
