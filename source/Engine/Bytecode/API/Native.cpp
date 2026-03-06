#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>

hsl_Object* hsl_native_new(hsl_Context* context, hsl_NativeFn native) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return nullptr;
	}

	ObjAPINative* nativeObj = manager->NewAPINative((APINativeFn)native);
	return (hsl_Object*)nativeObj;
}
#endif
