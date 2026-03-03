#include <stdio.h>
#include <stdlib.h>
#include <libhsl.h>

const char* code =\
	"event sum(a, b) {\n"\
	"    return a + b;\n"\
	"}\n"\
	"event sum_array_values(array, length) {\n"\
	"    var result = 0;\n"\
	"    for (var i = 0; i < length; i++) {\n"\
	"    	result += array[i];\n"\
	"    }\n"\
	"    return result;\n"\
	"}";

struct hsl_Thread* load_code(struct hsl_Context* context, const char* code) {
	struct hsl_CompilerSettings settings = {0};
	settings.show_warnings = true;
	settings.do_optimizations = true;

	char* out_bytecode = NULL;
	size_t out_size = 0;

	int result = hsl_compile(context, code, &settings, &out_bytecode, &out_size, NULL);
	if (result != HSL_OK) {
		printf("Could not compile code");

		const char* error = hsl_get_compile_error(context);
		if (error) {
			printf(": %s", error);
		}

		printf("\n");

		return NULL;
	}

	struct hsl_Thread* thread = hsl_new_thread(context);
	if (thread == NULL) {
		printf("hsl_new_thread failed\n");
		return NULL;
	}

	struct hsl_Module* module;
	result = hsl_load_bytecode(out_bytecode, out_size, thread, &module, NULL);
	if (result != HSL_OK) {
		printf("hsl_load_bytecode failed: %d\n", result);
		return NULL;
	}

	free(out_bytecode);

	struct hsl_Function* function = hsl_get_main_function(module);
	result = hsl_setup_call_frame(thread, function, 0);
	if (result != HSL_OK) {
		printf("hsl_setup_call_frame failed: %d\n", result);
		return NULL;
	}

	result = hsl_run_call_frame(thread);
	if (result != HSL_OK) {
		printf("hsl_run_call_frame failed: %d\n", result);
		return NULL;
	}

	return thread;
}

struct hsl_Value* call_function(struct hsl_Thread* thread, size_t num_args) {
	int call_result = hsl_call(thread, num_args);
	if (call_result != HSL_OK) {
		printf("Could not call function: %d\n", call_result);
		return NULL;
	}

	return hsl_get_result(thread);
}

int main(int argc, char** argv) {
	// Init library
	if (hsl_init() != HSL_OK) {
		printf("Could not initialize library\n");
		return -1;
	}

	// Create context
	struct hsl_Context* context = hsl_new_context();
	if (!context) {
		printf("Could not create context\n");
		hsl_finish();
		return -1;
	}

	// Create thread
	struct hsl_Thread* thread = load_code(context, code);
	if (!thread) {
		hsl_free_context(context);
		hsl_finish();
		return -1;
	}

	// Get "sum" function
	struct hsl_Object* function = hsl_get_global_as_object(context, "sum");
	if (!function) {
		printf("Could not get \"sum\" function\n");
		hsl_free_context(context);
		hsl_finish();
		return -1;
	}

	struct hsl_Value* result;
	struct hsl_Object* array;

	// Sum 2 + 2
	hsl_push_object(thread, function);
	hsl_push_integer(thread, 2);
	hsl_push_integer(thread, 2);
	result = call_function(thread, 2);
	printf("Result: %d\n", hsl_value_as_integer(result));

	// Concatenate "hello, " + "world!"
	hsl_push_object(thread, function);
	hsl_push_string(thread, "hello, ");
	hsl_push_string(thread, "world!");
	result = call_function(thread, 2);
	printf("Result: %s\n", hsl_value_as_string(result));

	// Create array
	hsl_push_integer(thread, 10);
	hsl_push_integer(thread, 20);
	hsl_push_integer(thread, 30);
	array = hsl_array_new_from_stack(thread, 3);

	// Sum array values
	function = hsl_get_global_as_object(context, "sum_array_values");
	if (!function) {
		printf("Could not get \"sum_array_values\" function\n");
		hsl_free_context(context);
		hsl_finish();
		return -1;
	}

	hsl_push_object(thread, function);
	hsl_push_object(thread, array);
	hsl_push_integer(thread, (int)hsl_array_size(array));
	result = call_function(thread, 2);
	printf("Result: %d\n", hsl_value_as_integer(result));

	// Finish
	hsl_free_thread(thread);
	hsl_free_context(context);
	hsl_finish();

	return 0;
}
