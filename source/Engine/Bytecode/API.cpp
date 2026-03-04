#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandaloneMain.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Exceptions/CompilerErrorException.h>
#include <Engine/Hashing/Murmur.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/Utilities/StringUtils.h>

static hsl_ValueType ValueTypeToAPIValueType(ValueType type) {
	switch (type) {
	case VAL_NULL:
		return HSL_VAL_NULL;
	case VAL_INTEGER:
		return HSL_VAL_INTEGER;
	case VAL_DECIMAL:
		return HSL_VAL_DECIMAL;
	case VAL_OBJECT:
		return HSL_VAL_OBJECT;
	case VAL_LINKED_INTEGER:
		return HSL_VAL_LINKED_INTEGER;
	case VAL_LINKED_DECIMAL:
		return HSL_VAL_LINKED_DECIMAL;
	case VAL_HITBOX:
		return HSL_VAL_HITBOX;
	case VAL_LOCATION:
		return HSL_VAL_LOCATION;
	default:
		return HSL_VAL_INVALID;
	}
}

static hsl_ObjType ObjTypeToAPIObjType(ObjType type) {
	switch (type) {
	case OBJ_STRING:
		return HSL_OBJ_STRING;
	case OBJ_ARRAY:
		return HSL_OBJ_ARRAY;
	case OBJ_MAP:
		return HSL_OBJ_MAP;
	case OBJ_FUNCTION:
		return HSL_OBJ_FUNCTION;
	case OBJ_BOUND_METHOD:
		return HSL_OBJ_BOUND_METHOD;
	case OBJ_MODULE:
		return HSL_OBJ_MODULE;
	case OBJ_CLOSURE:
		return HSL_OBJ_CLOSURE;
	case OBJ_UPVALUE:
		return HSL_OBJ_UPVALUE;
	case OBJ_CLASS:
		return HSL_OBJ_CLASS;
	case OBJ_NAMESPACE:
		return HSL_OBJ_NAMESPACE;
	case OBJ_ENUM:
		return HSL_OBJ_ENUM;
	case OBJ_INSTANCE:
		return HSL_OBJ_INSTANCE;
	case OBJ_ENTITY:
		return HSL_OBJ_ENTITY;
	case OBJ_NATIVE_FUNCTION:
	case OBJ_API_NATIVE_FUNCTION:
		return HSL_OBJ_NATIVE_FUNCTION;
	case OBJ_NATIVE_INSTANCE:
		return HSL_OBJ_NATIVE_INSTANCE;
	default:
		return HSL_OBJ_INVALID;
	}
}

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

hsl_Module* hsl_load_script(hsl_Context* context, const char* code, hsl_CompilerSettings* settings, const char* input_filename) {
	MemoryStream* memStream = nullptr;
	ObjModule* loadedModule = nullptr;
	ScriptManager* manager = (ScriptManager*)context;

	hsl_Result result = hsl_compile_internal(manager, code, settings, &memStream, input_filename);
	if (result != HSL_OK) {
		return nullptr;
	}

	Uint32 filenameHash = 0xABCDABCD;
	if (input_filename) {
		filenameHash = manager->MakeFilenameHash(input_filename);
	}

	memStream->Seek(0);
	loadedModule = manager->LoadBytecode(memStream, filenameHash);
	memStream->Close();

	return (hsl_Module*)loadedModule;
}

hsl_Module* hsl_load_bytecode(hsl_Context* context, char* code, size_t size, const char* input_filename) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !code || !size) {
		return nullptr;
	}

	Uint32 filenameHash = 0xABCDABCD;
	if (input_filename) {
		filenameHash = manager->MakeFilenameHash(input_filename);
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

size_t hsl_module_function_count(hsl_Module* module) {
	ObjModule* objModule = (ObjModule*)module;
	if (!objModule) {
		return 0;
	}

	return objModule->Functions->size();
}

hsl_Result hsl_module_get_function(hsl_Module* module, size_t index, hsl_Function** function) {
	ObjModule* objModule = (ObjModule*)module;
	ObjFunction* objFunction;

	if (!objModule) {
		return HSL_INVALID_ARGUMENT;
	}

	if (index >= objModule->Functions->size()) {
		return HSL_INVALID_ARGUMENT;
	}

	objFunction = (*objModule->Functions)[index];
	*function = (hsl_Function*)objFunction;

	return HSL_OK;
}

hsl_Function* hsl_module_get_main_function(hsl_Module* module) {
	ObjModule* objModule = (ObjModule*)module;
	if (!objModule) {
		return nullptr;
	}

	ObjFunction* objFunction = (*objModule->Functions)[0];
	return (hsl_Function*)objFunction;
}

hsl_Result hsl_setup_call_frame(hsl_Thread* thread, hsl_Function* function, size_t num_args) {
	VMThread* vmThread = (VMThread*)thread;
	ObjFunction* objFunction = (ObjFunction*)function;

	if (!vmThread || !objFunction) {
		return HSL_INVALID_ARGUMENT;
	}

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
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

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
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->FrameCount == 0) {
		return HSL_NO_CALL_FRAMES;
	}

	int ret = vmThread->RunInstruction();
	if (ret != INTERPRET_OK && ret != INTERPRET_FINISHED) {
		return HSL_RUNTIME_ERROR;
	}

	return HSL_OK;
}

hsl_Result hsl_call(hsl_Thread* thread, size_t num_args) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackTop - num_args - 1 < vmThread->Stack) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue callable = vmThread->Peek(num_args);
	if (!IS_CALLABLE(callable)) {
		return HSL_COULD_NOT_CALL;
	}

	if (!vmThread->CallValue(callable, num_args)) {
		return HSL_COULD_NOT_CALL;
	}

	if (IS_NATIVE_FUNCTION(callable) || IS_API_NATIVE_FUNCTION(callable)) {
		vmThread->InterpretResult = vmThread->Pop();

		return HSL_OK;
	}

	return hsl_run_call_frame(thread);
}

hsl_Value* hsl_get_result(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	return (hsl_Value*)(&vmThread->InterpretResult);
}

hsl_ValueType hsl_value_type(hsl_Value* value) {
	if (!value) {
		return HSL_VAL_INVALID;
	}

	return ValueTypeToAPIValueType((ValueType)((VMValue*)value)->Type);
}

hsl_ObjType hsl_object_type(hsl_Object* object) {
	if (!object) {
		return HSL_OBJ_INVALID;
	}

	return ObjTypeToAPIObjType((ObjType)((Obj*)object)->Type);
}

int hsl_value_as_integer(hsl_Value* value) {
	if (!value) {
		return 0;
	}

	VMValue vmValue = *(VMValue*)value;
	if (IS_INTEGER(vmValue)) {
		return AS_INTEGER(vmValue);
	}

	return 0;
}

float hsl_value_as_decimal(hsl_Value* value) {
	if (!value) {
		return 0.0f;
	}

	VMValue vmValue = *(VMValue*)value;
	if (IS_DECIMAL(vmValue)) {
		return AS_DECIMAL(vmValue);
	}

	return 0.0f;
}

char* hsl_value_as_string(hsl_Value* value) {
	if (!value) {
		return nullptr;
	}

	VMValue vmValue = *(VMValue*)value;
	if (IS_STRING(vmValue)) {
		return AS_CSTRING(vmValue);
	}

	return nullptr;
}

hsl_Object* hsl_value_as_object(hsl_Value* value) {
	if (!value) {
		return nullptr;
	}

	VMValue vmValue = *(VMValue*)value;
	if (IS_OBJECT(vmValue)) {
		return (hsl_Object*)AS_OBJECT(vmValue);
	}

	return nullptr;
}

hsl_Result hsl_push_integer(hsl_Thread* thread, int value) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	vmThread->Push(INTEGER_VAL(value));

	return HSL_OK;
}

hsl_Result hsl_push_decimal(hsl_Thread* thread, float value) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	vmThread->Push(DECIMAL_VAL(value));

	return HSL_OK;
}

hsl_Result hsl_push_string(hsl_Thread* thread, const char* value) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	if (vmThread->Manager->Lock()) {
		ObjString* string = vmThread->Manager->CopyString(value);

		vmThread->Push(OBJECT_VAL(string));

		vmThread->Manager->Unlock();

		return HSL_OK;
	}

	return HSL_COULD_NOT_ACQUIRE_LOCK;
}

hsl_Result hsl_push_sized_string(hsl_Thread* thread, const char* value, size_t sz) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || (!value && sz > 0)) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	if (vmThread->Manager->Lock()) {
		ObjString* string = vmThread->Manager->CopyString(value, sz);
		if (!string) {
			vmThread->Manager->Unlock();
			return HSL_OUT_OF_MEMORY;
		}

		vmThread->Push(OBJECT_VAL(string));

		vmThread->Manager->Unlock();

		return HSL_OK;
	}

	return HSL_COULD_NOT_ACQUIRE_LOCK;
}

hsl_Result hsl_push_object(hsl_Thread* thread, hsl_Object* object) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	vmThread->Push(OBJECT_VAL((Obj*)object));

	return HSL_OK;
}

hsl_Result hsl_push_null(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackSize() == STACK_SIZE_MAX) {
		return HSL_STACK_OVERFLOW;
	}

	vmThread->Push(NULL_VAL);

	return HSL_OK;
}

int hsl_pop_integer(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return 0;
	}

	if (vmThread->StackTop == vmThread->Stack) {
		return 0;
	}

	VMValue value = vmThread->Pop();
	if (IS_INTEGER(value)) {
		return AS_INTEGER(value);
	}

	return 0;
}

float hsl_pop_decimal(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return 0.0f;
	}

	if (vmThread->StackTop == vmThread->Stack) {
		return 0.0f;
	}

	VMValue value = vmThread->Pop();
	if (IS_DECIMAL(value)) {
		return AS_DECIMAL(value);
	}

	return 0.0f;
}

char* hsl_pop_string(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	if (vmThread->StackTop == vmThread->Stack) {
		return nullptr;
	}

	VMValue value = vmThread->Pop();
	if (IS_STRING(value)) {
		return StringUtils::Duplicate(AS_CSTRING(value));
	}

	return nullptr;
}

hsl_Object* hsl_pop_object(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	if (vmThread->StackTop == vmThread->Stack) {
		return nullptr;
	}

	VMValue value = vmThread->Pop();
	if (IS_OBJECT(value)) {
		return (hsl_Object*)AS_OBJECT(value);
	}

	return nullptr;
}

hsl_Result hsl_pop(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackTop == vmThread->Stack) {
		return HSL_STACK_UNDERFLOW;
	}

	vmThread->Pop();

	return HSL_OK;
}

hsl_Value* hsl_stack_get(hsl_Thread* thread, size_t index) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	VMValue* stackPointer = vmThread->StackTop - index - 1;
	if (stackPointer < vmThread->Stack) {
		return nullptr;
	}

	return (hsl_Value*)stackPointer;
}

hsl_Result hsl_stack_set(hsl_Thread* thread, hsl_Value* value, size_t index) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue* stackPointer = vmThread->StackTop - index - 1;
	if (stackPointer < vmThread->Stack) {
		return HSL_INVALID_ARGUMENT;
	}

	*stackPointer = *((VMValue*)value);

	return HSL_OK;
}

hsl_Result hsl_stack_copy(hsl_Thread* thread, size_t index, hsl_Value* dest) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue* stackPointer = vmThread->StackTop - index - 1;
	if (stackPointer < vmThread->Stack) {
		return HSL_INVALID_ARGUMENT;
	}

	memcpy((VMValue*)dest, stackPointer, sizeof(VMValue));

	return HSL_OK;
}

size_t hsl_stack_size(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return 0;
	}

	return vmThread->StackSize();
}

hsl_Result hsl_copy_integer(int value, hsl_Value* dest) {
	if (!dest) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue valueToCopy = INTEGER_VAL(value);

	memcpy((VMValue*)dest, &valueToCopy, sizeof(VMValue));

	return HSL_OK;
}

hsl_Result hsl_copy_decimal(float value, hsl_Value* dest) {
	if (!dest) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue valueToCopy = DECIMAL_VAL(value);

	memcpy((VMValue*)dest, &valueToCopy, sizeof(VMValue));

	return HSL_OK;
}

hsl_Result hsl_copy_object(hsl_Object* object, hsl_Value* dest) {
	if (!object || !dest) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue valueToCopy = OBJECT_VAL(object);

	memcpy((VMValue*)dest, &valueToCopy, sizeof(VMValue));

	return HSL_OK;
}

hsl_Result hsl_copy_null(hsl_Value* dest) {
	if (!dest) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue valueToCopy = NULL_VAL;

	memcpy((VMValue*)dest, &valueToCopy, sizeof(VMValue));

	return HSL_OK;
}

int hsl_global_exists(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	if (manager->Globals->Exists(name)) {
		return 1;
	}

	return 0;
}

hsl_ValueType hsl_global_type(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_VAL_INVALID;
	}

	if (!manager->Globals->Exists(name)) {
		return HSL_VAL_INVALID;
	}

	VMValue value = manager->Globals->Get(name);
	return ValueTypeToAPIValueType((ValueType)value.Type);
}

int hsl_get_global_as_integer(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	if (!manager->Globals->Exists(name)) {
		return HSL_DOES_NOT_EXIST;
	}

	VMValue value = manager->Globals->Get(name);
	if (IS_INTEGER(value)) {
		return AS_INTEGER(value);
	}

	return 0;
}

float hsl_get_global_as_decimal(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	if (!manager->Globals->Exists(name)) {
		return HSL_DOES_NOT_EXIST;
	}

	VMValue value = manager->Globals->Get(name);
	if (IS_DECIMAL(value)) {
		return AS_DECIMAL(value);
	}

	return 0.0f;
}

char* hsl_get_global_as_string(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return nullptr;
	}

	if (!manager->Globals->Exists(name)) {
		return nullptr;
	}

	VMValue value = manager->Globals->Get(name);
	if (IS_STRING(value)) {
		return AS_CSTRING(value);
	}

	return nullptr;
}

hsl_Object* hsl_get_global_as_object(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return nullptr;
	}

	if (!manager->Globals->Exists(name)) {
		return nullptr;
	}

	VMValue value = manager->Globals->Get(name);
	if (IS_OBJECT(value)) {
		return (hsl_Object*)AS_OBJECT(value);
	}

	return nullptr;
}

hsl_Result hsl_set_global(hsl_Context* context, const char* name, hsl_Value* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	manager->Globals->Put(name, *((VMValue*)value));

	return HSL_OK;
}

hsl_Result hsl_remove_global(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	if (!manager->Globals->Exists(name)) {
		return HSL_DOES_NOT_EXIST;
	}

	manager->Globals->Remove(name);

	return HSL_OK;
}

Uint32 hsl_get_hash_internal(const char* name) {
	return Murmur::EncryptString(name);
}

hsl_Result hsl_call_getter_internal(VMThread* thread, Obj* object, ValueGetFn getter, Uint32 hash, VMValue& result) {
	try {
		VMValue value;
		if (getter(object, hash, &value, thread)) {
			result = value;
			return HSL_OK;
		}
	} catch (const ScriptException& error) {
		return HSL_RUNTIME_ERROR;
	}

	return HSL_DOES_NOT_EXIST;
}

hsl_Result hsl_get_field_internal(VMThread* thread, VMValue object, Uint32 hash, bool callGetter, VMValue& result) {
	if (!thread->Manager->Lock()) {
		return HSL_COULD_NOT_ACQUIRE_LOCK;
	}

	VMValue value;

	hsl_Result callResult = HSL_DOES_NOT_EXIST;

	if (IS_INSTANCEABLE(object)) {
		ObjInstance* instance = AS_INSTANCE(object);
		if (instance->Fields->GetIfExists(hash, &value)) {
			result = Value::Delink(value);
			callResult = HSL_OK;
		}
		else if (callGetter) {
			callResult = hsl_call_getter_internal(thread, AS_OBJECT(object), instance->PropertyGet, hash, result);
		}
	}
	else if (IS_CLASS(object)) {
		ObjClass* klass = AS_CLASS(object);
		if (klass->Fields->GetIfExists(hash, &value)) {
			result = Value::Delink(value);
			callResult = HSL_OK;
		}
	}
	else if (IS_NAMESPACE(object)) {
		ObjNamespace* ns = AS_NAMESPACE(object);
		if (ns->Fields->GetIfExists(hash, &value)) {
			result = Value::Delink(result);
			callResult = HSL_OK;
		}
	}
	else if (callGetter && IS_OBJECT(object) && AS_OBJECT(object)->Class) {
		Obj* objPtr = AS_OBJECT(object);
		callResult = hsl_call_getter_internal(thread, AS_OBJECT(object), objPtr->Class->PropertyGet, hash, result);
	}
	else {
		callResult = HSL_INVALID_ARGUMENT;
	}

	thread->Manager->Unlock();

	return callResult;
}

int hsl_get_field_as_integer(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return 0;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, true, value);
	if (result != HSL_OK) {
		return 0;
	}

	if (IS_INTEGER(value)) {
		return AS_INTEGER(value);
	}

	return 0;
}

float hsl_get_field_as_decimal(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return 0.0f;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, true, value);
	if (result != HSL_OK) {
		return 0.0f;
	}

	if (IS_DECIMAL(value)) {
		return AS_DECIMAL(value);
	}

	return 0.0f;
}

char* hsl_get_field_as_string(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return nullptr;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, true, value);
	if (result != HSL_OK) {
		return nullptr;
	}

	if (IS_STRING(value)) {
		return AS_CSTRING(value);
	}

	return nullptr;
}

hsl_Object* hsl_get_field_as_object(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return nullptr;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, true, value);
	if (result != HSL_OK) {
		return nullptr;
	}

	if (IS_OBJECT(value)) {
		return (hsl_Object*)AS_OBJECT(value);
	}

	return nullptr;
}

hsl_Result hsl_push_field_to_stack(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, true, value);
	if (result != HSL_OK) {
		return result;
	}

	vmThread->Push(value);

	return HSL_OK;
}

hsl_ValueType hsl_get_field_type(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return HSL_VAL_INVALID;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, true, value);
	if (result != HSL_OK) {
		return HSL_VAL_INVALID;
	}

	return ValueTypeToAPIValueType((ValueType)value.Type);
}

int hsl_get_field_as_integer_direct(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return 0;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, false, value);
	if (result != HSL_OK) {
		return 0;
	}

	if (IS_INTEGER(value)) {
		return AS_INTEGER(value);
	}

	return 0;
}

float hsl_get_field_as_decimal_direct(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return 0.0f;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, false, value);
	if (result != HSL_OK) {
		return 0.0f;
	}

	if (IS_DECIMAL(value)) {
		return AS_DECIMAL(value);
	}

	return 0.0f;
}

char* hsl_get_field_as_string_direct(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return nullptr;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, false, value);
	if (result != HSL_OK) {
		return nullptr;
	}

	if (IS_STRING(value)) {
		return AS_CSTRING(value);
	}

	return nullptr;
}

hsl_Object* hsl_get_field_as_object_direct(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return nullptr;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, false, value);
	if (result != HSL_OK) {
		return nullptr;
	}

	if (IS_OBJECT(value)) {
		return (hsl_Object*)AS_OBJECT(value);
	}

	return nullptr;
}

hsl_Result hsl_push_field_to_stack_direct(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, false, value);
	if (result != HSL_OK) {
		return result;
	}

	vmThread->Push(value);

	return HSL_OK;
}

hsl_ValueType hsl_get_field_type_direct(hsl_Thread* thread, hsl_Object* object, const char* name) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return HSL_VAL_INVALID;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue value;

	hsl_Result result = hsl_get_field_internal(vmThread, OBJECT_VAL(object), hash, false, value);
	if (result != HSL_OK) {
		return HSL_VAL_INVALID;
	}

	return ValueTypeToAPIValueType((ValueType)value.Type);
}

hsl_Result hsl_invoke(hsl_Thread* thread, const char* name, size_t num_args) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackTop - num_args - 1 < vmThread->Stack) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	int status = vmThread->Invoke(vmThread->Peek(num_args), num_args, hash);
	if (status != INVOKE_OK) {
		return HSL_COULD_NOT_CALL;
	}

	return HSL_OK;
}

hsl_Result hsl_invoke_callable(hsl_Thread* thread, hsl_Object* callable, size_t num_args) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !callable) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue callableValue = OBJECT_VAL(callable);
	if (!IS_CALLABLE(callableValue)) {
		return HSL_COULD_NOT_CALL;
	}

	if (!vmThread->CallForObject(callableValue, num_args)) {
		return HSL_COULD_NOT_CALL;
	}

	return HSL_OK;
}

int hsl_callable_get_arity(hsl_Object* callable) {
	if (!callable) {
		return 0;
	}

	int minArity, maxArity;
	if (ScriptManager::GetArity(OBJECT_VAL(callable), minArity, maxArity)) {
		return maxArity;
	}
	return 0;
}

int hsl_callable_get_min_arity(hsl_Object* callable) {
	if (!callable) {
		return 0;
	}

	int minArity, maxArity;
	if (ScriptManager::GetArity(OBJECT_VAL(callable), minArity, maxArity)) {
		return minArity;
	}
	return 0;
}

hsl_Object* hsl_object_get_class(hsl_Object* object) {
	if (!object) {
		return nullptr;
	}

	return (hsl_Object*)((Obj*)object)->Class;
}

hsl_Object* hsl_native_new(hsl_Context* context, hsl_NativeFn native) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return nullptr;
	}

	if (manager->Lock()) {
		ObjAPINative* nativeObj = manager->NewAPINative((APINativeFn)native);
		manager->Unlock();
		return (hsl_Object*)nativeObj;
	}

	return nullptr;
}

hsl_Object* hsl_class_new(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return nullptr;
	}

	if (manager->Lock()) {
		ObjClass* klass = manager->NewClass(name);
		manager->Unlock();
		return (hsl_Object*)klass;
	}

	return nullptr;
}

int hsl_class_has_method(hsl_Object* object, const char* name) {
	if (!object || !name) {
		return 0;
	}

	if (((Obj*)object)->Type != OBJ_CLASS) {
		return 0;
	}

	ObjClass* klass = (ObjClass*)object;
	if (klass->Methods->Exists(name)) {
		return 1;
	}

	return 0;
}

hsl_Object* hsl_class_get_method(hsl_Object* object, const char* name) {
	if (!object || !name) {
		return nullptr;
	}

	if (((Obj*)object)->Type != OBJ_CLASS) {
		return nullptr;
	}

	ObjClass* klass = (ObjClass*)object;
	if (klass->Methods->Exists(name)) {
		VMValue callable = klass->Methods->Get(name);
		return (hsl_Object*)AS_OBJECT(callable);
	}

	return nullptr;
}

hsl_Result hsl_class_define_method(hsl_Object* object, const char* name, hsl_Function* function) {
	if (!object || !name || !function) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)object)->Type != OBJ_CLASS) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjClass* klass = (ObjClass*)object;
	ObjFunction* objFunction = (ObjFunction*)function;

	klass->Methods->Put(name, OBJECT_VAL(objFunction));

	objFunction->Class = klass;

	return HSL_OK;
}

hsl_Result hsl_class_define_native(hsl_Object* object, const char* name, hsl_Object* nativeObj) {
	if (!object || !name || !nativeObj) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)object)->Type != OBJ_CLASS) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)nativeObj)->Type != OBJ_API_NATIVE_FUNCTION) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjClass* klass = (ObjClass*)object;

	klass->Methods->Put(name, OBJECT_VAL(nativeObj));

	return HSL_OK;
}

int hsl_class_has_initializer(hsl_Object* object) {
	if (!object) {
		return 0;
	}

	if (((Obj*)object)->Type != OBJ_CLASS) {
		return 0;
	}

	ObjClass* klass = (ObjClass*)object;
	if (HasInitializer(klass)) {
		return 1;
	}

	return 0;
}

hsl_Object* hsl_class_get_initializer(hsl_Object* object) {
	if (!object) {
		return nullptr;
	}

	if (((Obj*)object)->Type != OBJ_CLASS) {
		return nullptr;
	}

	ObjClass* klass = (ObjClass*)object;
	if (!HasInitializer(klass)) {
		return nullptr;
	}

	return (hsl_Object*)AS_OBJECT(klass->Initializer);
}

hsl_Result hsl_class_set_initializer(hsl_Object* object, hsl_Function* initializer) {
	if (!object || !initializer) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)object)->Type != OBJ_CLASS) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjClass* klass = (ObjClass*)object;

	klass->Initializer = OBJECT_VAL(initializer);

	return HSL_OK;
}

hsl_Object* hsl_class_get_parent(hsl_Object* object) {
	if (!object) {
		return nullptr;
	}

	if (((Obj*)object)->Type != OBJ_CLASS) {
		return nullptr;
	}

	ObjClass* klass = (ObjClass*)object;

	return (hsl_Object*)klass->Parent;
}

hsl_Result hsl_class_set_parent(hsl_Object* object, hsl_Object* parent) {
	if (!object || !parent) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)object)->Type != OBJ_CLASS) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)parent)->Type != OBJ_CLASS) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjClass* klass = (ObjClass*)object;

	klass->Parent = (ObjClass*)parent;

	return HSL_OK;
}

Obj* hsl_instance_new_internal(VMThread* vmThread, ObjClass* classObj) {
	if (classObj->NewFn) {
		try {
			return classObj->NewFn(vmThread);
		} catch (const ScriptException& error) {
			vmThread->ThrowRuntimeError(false, "%s", error.what());
			return nullptr;
		}
	}
	else {
		return (Obj*)vmThread->Manager->NewInstance(classObj);
	}
}

hsl_Object* hsl_instance_new(hsl_Thread* thread, hsl_Object* klass) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !klass) {
		return nullptr;
	}

	if (((Obj*)klass)->Type != OBJ_CLASS) {
		return nullptr;
	}

	if (!vmThread->Manager->Lock()) {
		return nullptr;
	}

	Obj* instance = hsl_instance_new_internal(vmThread, (ObjClass*)klass);

	vmThread->Manager->Unlock();

	return (hsl_Object*)instance;
}

hsl_Result hsl_instance_new_from_stack(hsl_Thread* thread, size_t num_args) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return HSL_INVALID_ARGUMENT;
	}

	if (vmThread->StackTop - num_args - 1 < vmThread->Stack) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue callee = vmThread->Peek(num_args);
	if (!IS_OBJECT(callee) || OBJECT_TYPE(callee) != OBJ_CLASS) {
		return HSL_COULD_NOT_CALL;
	}

	if (!vmThread->Manager->Lock()) {
		return HSL_COULD_NOT_ACQUIRE_LOCK;
	}

	ObjClass* klass = AS_CLASS(callee);
	Obj* instance = hsl_instance_new_internal(vmThread, (ObjClass*)klass);
	if (instance == nullptr) {
		vmThread->StackTop[-num_args - 1] = NULL_VAL;
		vmThread->Manager->Unlock();
		return HSL_COULD_NOT_INSTANTIATE;
	}

	vmThread->StackTop[-num_args - 1] = OBJECT_VAL(instance);

	if (HasInitializer(klass)) {
		vmThread->Manager->Unlock();
		Obj* callable = AS_OBJECT(klass->Initializer);
		return hsl_invoke_callable(thread, (hsl_Object*)callable, num_args);
	}
	else if (num_args != 0) {
		vmThread->Manager->Unlock();
		return HSL_ARG_COUNT_MISMATCH;
	}

	vmThread->Manager->Unlock();
	return HSL_OK;
}

hsl_Object* hsl_array_new(hsl_Context* context) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return nullptr;
	}

	if (manager->Lock()) {
		ObjArray* array = manager->NewArray();
		manager->Unlock();
		return (hsl_Object*)array;
	}

	return nullptr;
}

hsl_Object* hsl_array_new_from_stack(hsl_Thread* thread, size_t count) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	if (vmThread->StackTop - count - 1 < vmThread->Stack) {
		return nullptr;
	}

	if (vmThread->Manager->Lock()) {
		ObjArray* array = vmThread->Manager->NewArray();
		array->Values->reserve(count);
		for (int i = (int)count - 1; i >= 0; i--) {
			array->Values->push_back(vmThread->Peek(i));
		}
		for (int i = (int)count - 1; i >= 0; i--) {
			vmThread->Pop();
		}
		vmThread->Manager->Unlock();
		return (hsl_Object*)array;
	}

	return nullptr;
}


hsl_Result hsl_array_push(hsl_Object* object, hsl_Value* value) {
	if (!object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)object)->Type != OBJ_ARRAY) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjArray* array = (ObjArray*)object;

	array->Values->push_back(*((VMValue*)value));

	return HSL_OK;
}

hsl_Result hsl_array_pop(hsl_Object* object) {
	if (!object) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)object)->Type != OBJ_ARRAY) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjArray* array = (ObjArray*)object;

	if (array->Values->size() == 0) {
		return HSL_INVALID_ARGUMENT;
	}

	array->Values->pop_back();

	return HSL_OK;
}

hsl_Value* hsl_array_get(hsl_Object* object, int index) {
	if (!object) {
		return nullptr;
	}

	if (((Obj*)object)->Type != OBJ_ARRAY) {
		return nullptr;
	}

	ObjArray* array = (ObjArray*)object;

	if (index < 0 || (Uint32)index >= array->Values->size()) {
		return nullptr;
	}

	return (hsl_Value*)(&(*array->Values)[index]);
}

hsl_Result hsl_array_set(hsl_Object* object, int index, hsl_Value* value) {
	if (!object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	if (((Obj*)object)->Type != OBJ_ARRAY) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjArray* array = (ObjArray*)object;

	if (index < 0 || (Uint32)index >= array->Values->size()) {
		return HSL_INVALID_ARGUMENT;
	}

	(*array->Values)[index] = *((VMValue*)value);

	return HSL_OK;
}

size_t hsl_array_size(hsl_Object* object) {
	if (!object) {
		return 0;
	}

	if (((Obj*)object)->Type != OBJ_ARRAY) {
		return 0;
	}

	ObjArray* array = (ObjArray*)object;

	return array->Values->size();
}
#endif
