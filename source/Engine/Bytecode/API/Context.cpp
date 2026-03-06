#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>

hsl_Context* hsl_context_new() {
	ScriptManager* manager = new ScriptManager;
	return (hsl_Context*)manager;
}

hsl_Result hsl_context_lock(hsl_Context* context) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return HSL_INVALID_ARGUMENT;
	}

	if (manager->Lock()) {
		return HSL_OK;
	}

	return HSL_COULD_NOT_ACQUIRE_LOCK;
}

hsl_Result hsl_context_unlock(hsl_Context* context) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return HSL_INVALID_ARGUMENT;
	}

	if (manager->Unlock()) {
		return HSL_OK;
	}

	return HSL_COULD_NOT_RELEASE_LOCK;
}

hsl_Result hsl_context_free(hsl_Context* context) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return HSL_INVALID_ARGUMENT;
	}

	delete manager;

	return HSL_OK;
}
#endif
