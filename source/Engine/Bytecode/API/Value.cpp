#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>

hsl_ValueType ValueTypeToAPIValueType(ValueType type) {
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

hsl_ObjType ObjTypeToAPIObjType(ObjType type) {
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
#endif
