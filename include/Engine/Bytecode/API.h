#ifndef LIBHSL_H
#define LIBHSL_H

#ifdef __cplusplus
extern "C" {
#endif

// This API is a proof of concept, and anything is subject to change.

#include <stddef.h>

#define HSL_FRAMES_MAX 64
#define HSL_FRAME_STACK_MAX 256 // Cannot be higher than this
#define HSL_STACK_SIZE_MAX (HSL_FRAMES_MAX * HSL_FRAME_STACK_MAX)

#define HSL_WITH_ITERATION_MAX 16

#define HSL_VM_THREAD_NAME_MAX 64

#define HSL_DEFAULT_BRANCH_LIMIT 100000

enum hsl_Result {
	HSL_OK,
	HSL_INVALID_ARGUMENT,
	HSL_OUT_OF_MEMORY,
	HSL_COULD_NOT_ACQUIRE_LOCK,
	HSL_COMPILE_ERROR,
	HSL_ARG_COUNT_MISMATCH,
	HSL_NOT_ENOUGH_ARGS,
	HSL_CALL_FRAME_OVERFLOW,
	HSL_NO_CALL_FRAMES,
	HSL_COULD_NOT_CALL,
	HSL_RUNTIME_ERROR,
	HSL_FATAL_ERROR,
	HSL_SCRIPT_ERROR,
	HSL_HIT_BRANCH_LIMIT,
	HSL_STACK_OVERFLOW,
	HSL_STACK_UNDERFLOW,
	HSL_DOES_NOT_EXIST,
	HSL_COULD_NOT_INSTANTIATE
};

enum hsl_ValueType {
	HSL_VAL_INVALID,
	HSL_VAL_NULL,
	HSL_VAL_INTEGER,
	HSL_VAL_DECIMAL,
	HSL_VAL_OBJECT,
	HSL_VAL_LINKED_INTEGER,
	HSL_VAL_LINKED_DECIMAL,
	HSL_VAL_HITBOX,
	HSL_VAL_LOCATION
};

enum hsl_ObjType {
	HSL_OBJ_INVALID,
	HSL_OBJ_STRING,
	HSL_OBJ_ARRAY,
	HSL_OBJ_MAP,
	HSL_OBJ_FUNCTION,
	HSL_OBJ_BOUND_METHOD,
	HSL_OBJ_MODULE,
	HSL_OBJ_CLOSURE,
	HSL_OBJ_UPVALUE,
	HSL_OBJ_CLASS,
	HSL_OBJ_NAMESPACE,
	HSL_OBJ_ENUM,
	HSL_OBJ_INSTANCE,
	HSL_OBJ_ENTITY,
	HSL_OBJ_NATIVE_FUNCTION,
	HSL_OBJ_NATIVE_INSTANCE
};

enum hsl_ErrorResponse {
	HSL_ERROR_RES_EXIT,
	HSL_ERROR_RES_CONTINUE
};

enum hsl_WithState {
	HSL_WITH_STATE_INIT,
	HSL_WITH_STATE_ITERATE,
	HSL_WITH_STATE_FINISH
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

struct hsl_Context;
struct hsl_Thread;
struct hsl_Module;
struct hsl_Function;
struct hsl_Value;
struct hsl_Object;

struct hsl_CompilerSettings {
	int show_warnings;
	int write_debug_info;
	int write_source_filename;
	int do_optimizations;
};

typedef int (*hsl_ImportScriptHandler)(const char* name, struct hsl_Thread* thread);
typedef int (*hsl_ImportClassHandler)(const char* name, struct hsl_Thread* thread);
typedef int (*hsl_WithIteratorHandler)(int state, struct hsl_Value* receiver, int* index, struct hsl_Value* new_receiver);
typedef enum hsl_ErrorResponse (*hsl_RuntimeErrorHandler)(enum hsl_Result result, const char* text);
typedef void (*hsl_LogCallback)(int level, const char* text);

typedef int (*hsl_NativeFn)(int num_args, struct hsl_Value* args, struct hsl_Thread* thread, struct hsl_Value* result);

// Returns the HSL version the library was compiled with.
const char* hsl_get_version();

// Initializes the library.
enum hsl_Result hsl_init();
// Disposes of the library.
void hsl_finish();

// Creates a context.
struct hsl_Context* hsl_new_context();
// Disposes of a context.
enum hsl_Result hsl_free_context(struct hsl_Context* context);

// Sets the directory where scripts are located. This is only used for the debugger.
enum hsl_Result hsl_set_scripts_directory(const char* directory);
// Sets the handler for 'import from' statements.
enum hsl_Result hsl_set_import_script_handler(struct hsl_Context* context, hsl_ImportScriptHandler handler);
// Sets the handler for 'import' statements.
enum hsl_Result hsl_set_import_class_handler(struct hsl_Context* context, hsl_ImportClassHandler handler);
// Sets the handler for 'with' iterators.
enum hsl_Result hsl_set_with_iterator_handler(struct hsl_Context* context, hsl_WithIteratorHandler handler);
// Sets the runtime error handler.
enum hsl_Result hsl_set_runtime_error_handler(struct hsl_Context* context, hsl_RuntimeErrorHandler handler);
// Sets the log callback.
enum hsl_Result hsl_set_log_callback(hsl_LogCallback callback);

// Creates a new thread.
struct hsl_Thread* hsl_new_thread(struct hsl_Context* context);
// Disposes of a thread.
void hsl_free_thread(struct hsl_Thread* thread);

// Gets the context that a thread belongs to.
struct hsl_Context* hsl_get_thread_context(struct hsl_Thread* thread);

// Compiles a script into bytecode.
enum hsl_Result hsl_compile(struct hsl_Context* context, const char* code, struct hsl_CompilerSettings* settings, char** out_bytecode, size_t *out_size, const char* input_filename);
// Compiles a script into bytecode, and loads the bytecode.
struct hsl_Module* hsl_load_script(struct hsl_Context* context, const char* code, struct hsl_CompilerSettings* settings, const char* input_filename);
// Loads bytecode.
struct hsl_Module* hsl_load_bytecode(struct hsl_Context* context, char* code, size_t size, const char* input_filename);

// Returns the last compilation error.
const char* hsl_get_compile_error(struct hsl_Context* context);

// Runs a module.
enum hsl_Result hsl_run_module(struct hsl_Thread* thread, struct hsl_Module* module);
// Returns the amount of functions in the module.
size_t hsl_module_function_count(struct hsl_Module* module);
// Returns a function from the module by index.
enum hsl_Result hsl_module_get_function(struct hsl_Module* module, size_t index, struct hsl_Function** function);
// Returns the main function of the module.
struct hsl_Function* hsl_module_get_main_function(struct hsl_Module* module);

// Sets up a new call frame for the thread.
enum hsl_Result hsl_setup_call_frame(struct hsl_Thread* thread, struct hsl_Function* function, size_t num_args);
// Runs code in the thread's current frame until it returns.
enum hsl_Result hsl_run_call_frame(struct hsl_Thread* thread);
// Runs the current instruction in the thread's current frame.
enum hsl_Result hsl_run_call_frame_instruction(struct hsl_Thread* thread);
// Calls a value in the stack, passing the values in the stack as arguments.
enum hsl_Result hsl_call(struct hsl_Thread* thread, size_t num_args);
// Gets the return value of a call to a function.
struct hsl_Value* hsl_get_result(struct hsl_Thread* thread);

// Gets the type of a value.
enum hsl_ValueType hsl_value_type(struct hsl_Value* value);
// Gets the type of an object.
enum hsl_ObjType hsl_object_type(struct hsl_Object* object);

// Converts a value to an integer.
int hsl_value_as_integer(struct hsl_Value* value);
// Converts a value to a decimal.
float hsl_value_as_decimal(struct hsl_Value* value);
// Converts a value to a string. This does not return a copy of the string.
char* hsl_value_as_string(struct hsl_Value* value);
// Converts a value to an object.
struct hsl_Object* hsl_value_as_object(struct hsl_Value* value);

// Pushes an integer to the stack.
enum hsl_Result hsl_push_integer(struct hsl_Thread* thread, int value);
// Pushes a decimal to the stack.
enum hsl_Result hsl_push_decimal(struct hsl_Thread* thread, float value);
// Pushes a string to the stack.
enum hsl_Result hsl_push_string(struct hsl_Thread* thread, const char* value);
// Pushes a string of a specific length to the stack.
enum hsl_Result hsl_push_string_sized(struct hsl_Thread* thread, const char* value, size_t sz);
// Pushes an object to the stack.
enum hsl_Result hsl_push_object(struct hsl_Thread* thread, struct hsl_Object* object);
// Pushes null to the stack.
enum hsl_Result hsl_push_null(struct hsl_Thread* thread);

// Pops an integer from the stack.
int hsl_pop_integer(struct hsl_Thread* thread);
// Pops a decimal from the stack.
float hsl_pop_decimal(struct hsl_Thread* thread);
// Pops a string from the stack. This returns a copy of the string.
char* hsl_pop_string(struct hsl_Thread* thread);
// Pops an object from the stack.
struct hsl_Object* hsl_pop_object(struct hsl_Thread* thread);
// Pops a value from the stack.
enum hsl_Result hsl_pop(struct hsl_Thread* thread);

// Gets a pointer to a specific stack location.
struct hsl_Value* hsl_stack_get(struct hsl_Thread* thread, size_t index);
// Copies a value to a specific stack location.
enum hsl_Result hsl_stack_set(struct hsl_Thread* thread, struct hsl_Value* value, size_t index);
// Copies a value from a specific stack location into a pointer to a value.
enum hsl_Result hsl_stack_copy(struct hsl_Thread* thread, size_t index, struct hsl_Value* dest);
// Gets the stack size.
size_t hsl_stack_size(struct hsl_Thread* thread);

// Copies a integer into a pointer to a value.
enum hsl_Result hsl_copy_integer(int value, struct hsl_Value* dest);
// Copies a decimal into a pointer to a value.
enum hsl_Result hsl_copy_decimal(float value, struct hsl_Value* dest);
// Copies an object into a pointer to a value.
enum hsl_Result hsl_copy_object(struct hsl_Object* object, struct hsl_Value* dest);
// Copies null into a pointer to a value.
enum hsl_Result hsl_copy_null(struct hsl_Value* dest);

// Returns 1 if the global exists, 0 if not.
int hsl_global_exists(struct hsl_Context* context, const char* name);
// Gets the type of a global.
enum hsl_ValueType hsl_global_type(struct hsl_Context* context, const char* name);
// Gets a global as an integer.
int hsl_global_as_integer(struct hsl_Context* context, const char* name);
// Gets a global as a decimal.
float hsl_global_as_decimal(struct hsl_Context* context, const char* name);
// Gets a global as a string. This does not return a copy of the string.
char* hsl_global_as_string(struct hsl_Context* context, const char* name);
// Gets a global as an object.
struct hsl_Object* hsl_global_as_object(struct hsl_Context* context, const char* name);
// Sets a global.
enum hsl_Result hsl_global_set(struct hsl_Context* context, const char* name, struct hsl_Value* value);
// Sets a global to an integer.
enum hsl_Result hsl_global_set_integer(struct hsl_Context* context, const char* name, int value);
// Sets a global to a decimal.
enum hsl_Result hsl_global_set_decimal(struct hsl_Context* context, const char* name, float value);
// Sets a global to a string.
enum hsl_Result hsl_global_set_string(struct hsl_Context* context, const char* name, const char* value);
// Sets a global to a string of a specific length.
enum hsl_Result hsl_global_set_string_sized(struct hsl_Context* context, const char* name, const char* value, size_t sz);
// Sets a global to an object.
enum hsl_Result hsl_global_set_object(struct hsl_Context* context, const char* name, struct hsl_Object* value);
// Replaces a global directly.
enum hsl_Result hsl_global_replace(struct hsl_Context* context, const char* name, struct hsl_Value* value);
// Replaces a global directly with an integer.
enum hsl_Result hsl_global_replace_with_integer(struct hsl_Context* context, const char* name, int value);
// Replaces a global directly with a decimal.
enum hsl_Result hsl_global_replace_with_decimal(struct hsl_Context* context, const char* name, float value);
// Replaces a global directly with a string.
enum hsl_Result hsl_global_replace_with_string(struct hsl_Context* context, const char* name, const char* value);
// Replaces a global directly with a string of a specific length.
enum hsl_Result hsl_global_replace_with_string_sized(struct hsl_Context* context, const char* name, const char* value, size_t sz);
// Replaces a global directly with an object.
enum hsl_Result hsl_global_replace_with_object(struct hsl_Context* context, const char* name, struct hsl_Object* value);
// Removes a global.
enum hsl_Result hsl_global_remove(struct hsl_Context* context, const char* name);

// Returns 1 if the constant exists, 0 if not.
int hsl_constant_exists(struct hsl_Context* context, const char* name);
// Gets the type of a constant.
enum hsl_ValueType hsl_constant_type(struct hsl_Context* context, const char* name);
// Gets a constant as an integer.
int hsl_constant_as_integer(struct hsl_Context* context, const char* name);
// Gets a constant as a decimal.
float hsl_constant_as_decimal(struct hsl_Context* context, const char* name);
// Gets a constant as a string. This does not return a copy of the string.
char* hsl_constant_as_string(struct hsl_Context* context, const char* name);
// Gets a constant as an object.
struct hsl_Object* hsl_constant_as_object(struct hsl_Context* context, const char* name);
// Defines a constant.
enum hsl_Result hsl_constant_define(struct hsl_Context* context, const char* name, struct hsl_Value* value);
// Defines an integer constant.
enum hsl_Result hsl_constant_define_integer(struct hsl_Context* context, const char* name, int value);
// Defines a decimal constant.
enum hsl_Result hsl_constant_define_decimal(struct hsl_Context* context, const char* name, float value);
// Defines a string constant.
enum hsl_Result hsl_constant_define_string(struct hsl_Context* context, const char* name, const char* value);
// Defines a string constant of a specific length.
enum hsl_Result hsl_constant_define_string_sized(struct hsl_Context* context, const char* name, const char* value, size_t sz);
// Defines an object constant.
enum hsl_Result hsl_constant_define_object(struct hsl_Context* context, const char* name, struct hsl_Object* value);
// Removes a constant.
enum hsl_Result hsl_constant_remove(struct hsl_Context* context, const char* name);

// Returns 1 if the object has the given field, 0 if it does not. Calls any getters.
int hsl_field_exists(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Gets a field of an object as an integer. Calls any getters.
int hsl_field_as_integer(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Gets a field of an object as a decimal. Calls any getters.
float hsl_field_as_decimal(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Gets a field of an object as a string. Calls any getters.
char* hsl_field_as_string(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Gets a field of an object as an object. Calls any getters.
struct hsl_Object* hsl_field_as_object(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Pushes a field of an object into the stack. Calls any getters.
enum hsl_Result hsl_field_push_to_stack(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Gets the type of a field of an object. Calls any getters.
enum hsl_ValueType hsl_field_type(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);

// Returns 1 if the object has the given field, 0 if it does not. Doesn't call any getters.
int hsl_field_exists_direct(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Gets a field of an object as an integer. Doesn't call any getters.
int hsl_field_as_integer_direct(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Gets a field of an object as a decimal. Doesn't call any getters.
float hsl_field_as_decimal_direct(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Gets a field of an object as a string. Doesn't call any getters.
char* hsl_field_as_string_direct(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Gets a field of an object as an object. Doesn't call any getters.
struct hsl_Object* hsl_field_as_object_direct(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Pushes a field of an object into the stack. Doesn't call any getters.
enum hsl_Result hsl_field_push_to_stack_direct(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);
// Gets the type of a field of an object. Doesn't call any getters.
enum hsl_ValueType hsl_field_type_direct(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);

// Sets a field of an object. Calls setters.
enum hsl_Result hsl_field_set(struct hsl_Object* object, const char* name, struct hsl_Value* value, struct hsl_Thread* thread);
// Sets a field of an object to an integer. Calls setters.
enum hsl_Result hsl_field_set_integer(struct hsl_Object* object, const char* name, int value, struct hsl_Thread* thread);
// Sets a field of an object to a decimal. Calls setters.
enum hsl_Result hsl_field_set_decimal(struct hsl_Object* object, const char* name, float value, struct hsl_Thread* thread);
// Sets a field of an object to a string. Calls setters.
enum hsl_Result hsl_field_set_string(struct hsl_Object* object, const char* name, const char* value, struct hsl_Thread* thread);
// Sets a field of an object to a string of a specific length. Calls setters.
enum hsl_Result hsl_field_set_string_sized(struct hsl_Object* object, const char* name, const char* value, size_t sz, struct hsl_Thread* thread);
// Sets a field of an object to an object. Calls setters.
enum hsl_Result hsl_field_set_object(struct hsl_Object* object, const char* name, struct hsl_Object* value, struct hsl_Thread* thread);

// Replaces a field of an object directly. Doesn't call any setters.
enum hsl_Result hsl_field_replace(struct hsl_Object* object, const char* name, struct hsl_Value* value, struct hsl_Thread* thread);
// Replaces a field of an object with an integer directly. Doesn't call any setters.
enum hsl_Result hsl_field_replace_with_integer(struct hsl_Object* object, const char* name, int value, struct hsl_Thread* thread);
// Replaces a field of an object with a decimal directly. Doesn't call any setters.
enum hsl_Result hsl_field_replace_with_decimal(struct hsl_Object* object, const char* name, float value, struct hsl_Thread* thread);
// Replaces a field of an object with a string directly. Doesn't call any setters.
enum hsl_Result hsl_field_replace_with_string(struct hsl_Object* object, const char* name, const char* value, struct hsl_Thread* thread);
// Replaces a field of an object with a string of a specific length directly. Doesn't call any setters.
enum hsl_Result hsl_field_replace_with_string_sized(struct hsl_Object* object, const char* name, const char* value, size_t sz, struct hsl_Thread* thread);
// Replaces a field of an object with an object directly. Doesn't call any setters.
enum hsl_Result hsl_field_replace_with_object(struct hsl_Object* object, const char* name, struct hsl_Object* value, struct hsl_Thread* thread);

// Removes a field from an object.
enum hsl_Result hsl_remove_field(struct hsl_Object* object, const char* name, struct hsl_Thread* thread);

// Invokes a callable by name for an instance on the stack.
enum hsl_Result hsl_invoke(struct hsl_Thread* thread, const char* name, size_t num_args);
// Invokes a callable directly for an instance on the stack.
enum hsl_Result hsl_invoke_callable(struct hsl_Thread* thread, struct hsl_Object* callable, size_t num_args);

// Gets the arity of a callable.
int hsl_callable_get_arity(struct hsl_Object* callable);
// Gets the minimum arity of a callable.
int hsl_callable_get_min_arity(struct hsl_Object* callable);

// Gets the class of the object.
struct hsl_Object* hsl_object_get_class(struct hsl_Object* object);

// Creates a new native function.
struct hsl_Object* hsl_native_new(struct hsl_Context* context, hsl_NativeFn native);

// Creates a new class.
struct hsl_Object* hsl_class_new(struct hsl_Context* context, const char* name);
// Returns 1 if the class has the given method, 0 if it does not.
int hsl_class_has_method(struct hsl_Object* object, const char* name);
// Gets a method in the class.
struct hsl_Object* hsl_class_get_method(struct hsl_Object* object, const char* name);
// Defines a method in the class.
enum hsl_Result hsl_class_define_method(struct hsl_Object* object, const char* name, struct hsl_Function* function);
// Defines a native function in the class.
enum hsl_Result hsl_class_define_native(struct hsl_Object* object, const char* name, struct hsl_Object* nativeObj);
// Returns 1 if the class has an initializer, 0 if it does not.
int hsl_class_has_initializer(struct hsl_Object* object);
// Returns the initializer of the class.
struct hsl_Object* hsl_class_get_initializer(struct hsl_Object* object);
// Defines the initializer of the class.
enum hsl_Result hsl_class_set_initializer(struct hsl_Object* object, struct hsl_Object* function);
// Returns the parent of the class.
struct hsl_Object* hsl_class_get_parent(struct hsl_Object* object);
// Defines the parent of the class.
enum hsl_Result hsl_class_set_parent(struct hsl_Object* object, struct hsl_Object* parent);

// Instantiates a class. Doesn't call the initializer.
struct hsl_Object* hsl_instance_new(struct hsl_Thread* thread, struct hsl_Object* klass);
// Instantiates a class on the stack, passing the values from the stack to the initializer.
enum hsl_Result hsl_instance_new_from_stack(struct hsl_Thread* thread, size_t num_args);

// Creates a new array.
struct hsl_Object* hsl_array_new(struct hsl_Context* context);
// Creates a new array, popping the values from the stack into the array.
struct hsl_Object* hsl_array_new_from_stack(struct hsl_Thread* thread, size_t count);
// Pushes a copy of a value into the array.
enum hsl_Result hsl_array_push(struct hsl_Object* object, struct hsl_Value* value);
// Pops a value from the array.
enum hsl_Result hsl_array_pop(struct hsl_Object* object, struct hsl_Value* value);
// Gets a pointer to a specific position in the array.
struct hsl_Value* hsl_array_get(struct hsl_Object* object, int index);
// Copies a value into a specific position in the array.
enum hsl_Result hsl_array_set(struct hsl_Object* object, int index, struct hsl_Value* value);
// Gets the size of the array.
size_t hsl_array_size(struct hsl_Object* object);

#ifdef __cplusplus
}
#endif

#endif /* LIBHSL_H */
