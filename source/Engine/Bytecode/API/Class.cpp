#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>

hsl_Object* hsl_class_new(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return nullptr;
	}

	ObjClass* klass = manager->NewClass(name);
	return (hsl_Object*)klass;
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
#endif
