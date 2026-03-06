#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandaloneMain.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Hashing/Murmur.h>

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

hsl_Result hsl_set_scripts_directory(const char* directory) {
	SetScriptsDirectory(directory);

	return HSL_OK;
}

hsl_Result hsl_set_lock_function(hsl_LockFunction function) {
	SetScriptManagerLockFunction(function);

	return HSL_OK;
}

hsl_Result hsl_set_unlock_function(hsl_UnlockFunction function) {
	SetScriptManagerUnlockFunction(function);

	return HSL_OK;
}

hsl_Result hsl_set_log_callback(hsl_LogCallback callback) {
	Log::SetCallback(callback);

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

Uint32 hsl_get_hash_internal(const char* name) {
	return Murmur::EncryptString(name);
}
#endif
