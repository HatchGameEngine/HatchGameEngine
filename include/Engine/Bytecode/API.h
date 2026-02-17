#ifndef LIBHSL_H
#define LIBHSL_H

#ifdef __cplusplus
extern "C" {
#endif

// This API is a proof of concept, and anything is subject to change.

#include <stddef.h>

enum hsl_Result {
    HSL_OK,
    HSL_INVALID_ARGUMENT,
    HSL_OUT_OF_MEMORY,
    HSL_COMPILE_ERROR,
    HSL_BYTECODE_LOAD_ERROR,
    HSL_TOO_MANY_THREADS,
    HSL_ARG_COUNT_MISMATCH,
    HSL_NOT_ENOUGH_ARGS,
    HSL_CALL_FRAME_OVERFLOW,
    HSL_NO_CALL_FRAMES,
    HSL_COULD_NOT_CALL,
    HSL_RUNTIME_ERROR,
    HSL_FATAL_ERROR,
    HSL_SCRIPT_ERROR,
    HSL_HIT_BRANCH_LIMIT
};

enum hsl_ErrorResponse {
    HSL_ERROR_RES_EXIT,
    HSL_ERROR_RES_CONTINUE
};

enum hsl_LogSeverity {
    HSL_LOG_VERBOSE = -1,
    HSL_LOG_INFO,
    HSL_LOG_WARN,
    HSL_LOG_ERROR,
    HSL_LOG_IMPORTANT,
    HSL_LOG_FATAL,
    HSL_LOG_API
};

struct hsl_CompilerSettings {
    int show_warnings;
    int write_debug_info;
    int write_source_filename;
    int do_optimizations;
};

struct hsl_Thread;
struct hsl_Module;
struct hsl_Function;

typedef int (*hsl_ImportScriptHandler)(const char* name, struct hsl_Thread* thread);
typedef int (*hsl_ImportClassHandler)(const char* name, struct hsl_Thread* thread);
typedef enum hsl_ErrorResponse (*hsl_RuntimeErrorHandler)(enum hsl_Result result, const char* text);
typedef void (*hsl_LogCallback)(int level, const char* text);

// Initializes the library.
void hsl_init();
// Disposes of the library.
void hsl_finish();

// Returns the HSL version the library was compiled with.
const char* hsl_get_version();
// Returns the last compilation error.
const char* hsl_get_compile_error();

// Sets the directory where scripts are located. This is only used for the debugger.
void hsl_set_scripts_directory(const char* directory);
// Sets the handler for 'import from' statements.
void hsl_set_import_script_handler(hsl_ImportScriptHandler handler);
// Sets the handler for 'import' statements.
void hsl_set_import_class_handler(hsl_ImportClassHandler handler);
// Sets a log callback.
void hsl_set_log_callback(hsl_LogCallback callback);
// Sets the runtime error handler.
void hsl_set_runtime_error_handler(hsl_RuntimeErrorHandler handler);

// Creates a new thread.
enum hsl_Result hsl_new_thread(struct hsl_Thread** thread);
// Disposes of a thread.
void hsl_free_thread(struct hsl_Thread* thread);

// Compiles a script into bytecode.
enum hsl_Result hsl_compile(const char* code, struct hsl_CompilerSettings* settings, char** out_bytecode, size_t *out_size, const char* input_filename);
// Compiles a script into bytecode, and loads the bytecode.
enum hsl_Result hsl_load_script(const char* code, struct hsl_CompilerSettings* settings, struct hsl_Thread* thread, struct hsl_Module** module, const char* input_filename);
// Loads bytecode.
enum hsl_Result hsl_load_bytecode(char* code, size_t size, struct hsl_Thread* thread, struct hsl_Module** module, const char* input_filename);

// Returns the amount of functions in the module.
int hsl_get_module_function_count(struct hsl_Module* module);
// Returns a function from the module by index.
enum hsl_Result hsl_get_module_function(struct hsl_Module* module, size_t index, struct hsl_Function** function);
// Returns the main function of the module.
struct hsl_Function* hsl_get_main_function(struct hsl_Module* module);

// Sets up a new call frame for the thread.
enum hsl_Result hsl_setup_call_frame(struct hsl_Thread* thread, struct hsl_Function* function, int num_args);
// Runs code in the thread's current frame until it returns.
enum hsl_Result hsl_run_call_frame(struct hsl_Thread* thread);
// Runs the current instruction in the thread's current frame.
enum hsl_Result hsl_run_call_frame_instruction(struct hsl_Thread* thread);

#ifdef __cplusplus
}
#endif

#endif /* LIBHSL_H */
