#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>

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
#endif
