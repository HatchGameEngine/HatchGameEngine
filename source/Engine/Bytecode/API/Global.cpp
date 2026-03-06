#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/Value.h>

hsl_Result hsl_table_set_internal(ScriptManager* manager, Table* table, const char* name, VMValue value, bool replace) {
	if (replace) {
		table->Put(name, value);
		return HSL_OK;
	}

	hsl_Result result = HSL_OK;

	VMValue LHS = table->Get(name);
	switch (LHS.Type) {
	case VAL_LINKED_INTEGER:
		value = Value::CastAsInteger(value);
		if (IS_NULL(value)) {
			result = HSL_INVALID_ARGUMENT;
			break;
		}
		AS_LINKED_INTEGER(LHS) = AS_INTEGER(value);
		break;
	case VAL_LINKED_DECIMAL:
		value = Value::CastAsDecimal(value);
		if (IS_NULL(value)) {
			result = HSL_INVALID_ARGUMENT;
			break;
		}
		AS_LINKED_DECIMAL(LHS) = AS_DECIMAL(value);
		break;
	default:
		table->Put(name, value);
		break;
	}

	return result;
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

int hsl_global_as_integer(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return 0;
	}

	if (!manager->Globals->Exists(name)) {
		return 0;
	}

	VMValue value = manager->Globals->Get(name);
	if (IS_INTEGER(value)) {
		return AS_INTEGER(value);
	}

	return 0;
}

float hsl_global_as_decimal(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return 0.0f;
	}

	if (!manager->Globals->Exists(name)) {
		return 0.0f;
	}

	VMValue value = manager->Globals->Get(name);
	if (IS_DECIMAL(value)) {
		return AS_DECIMAL(value);
	}

	return 0.0f;
}

char* hsl_global_as_string(hsl_Context* context, const char* name) {
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

hsl_Object* hsl_global_as_object(hsl_Context* context, const char* name) {
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

hsl_Result hsl_global_set(hsl_Context* context, const char* name, hsl_Value* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue vmValue = *(VMValue*)value;

	return hsl_table_set_internal(manager, manager->Globals, name, vmValue, false);
}

hsl_Result hsl_global_set_integer(hsl_Context* context, const char* name, int value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, INTEGER_VAL(value), false);
}

hsl_Result hsl_global_set_decimal(hsl_Context* context, const char* name, float value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, DECIMAL_VAL(value), false);
}

hsl_Result hsl_global_set_string(hsl_Context* context, const char* name, const char* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjString* string = manager->CopyString(value);
	if (!string) {
		return HSL_OUT_OF_MEMORY;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, OBJECT_VAL(string), false);
}

hsl_Result hsl_global_set_string_sized(hsl_Context* context, const char* name, const char* value, size_t sz) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || (!value && sz > 0)) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjString* string = manager->CopyString(value, sz);
	if (!string) {
		return HSL_OUT_OF_MEMORY;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, OBJECT_VAL(string), false);
}

hsl_Result hsl_global_set_object(hsl_Context* context, const char* name, hsl_Object* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, OBJECT_VAL(value), false);
}

hsl_Result hsl_global_replace(hsl_Context* context, const char* name, hsl_Value* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue vmValue = *(VMValue*)value;

	return hsl_table_set_internal(manager, manager->Globals, name, vmValue, true);
}

hsl_Result hsl_global_replace_with_integer(hsl_Context* context, const char* name, int value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, INTEGER_VAL(value), true);
}

hsl_Result hsl_global_replace_with_decimal(hsl_Context* context, const char* name, float value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, DECIMAL_VAL(value), true);
}

hsl_Result hsl_global_replace_with_string(hsl_Context* context, const char* name, const char* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjString* string = manager->CopyString(value);
	if (!string) {
		return HSL_OUT_OF_MEMORY;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, OBJECT_VAL(string), true);
}

hsl_Result hsl_global_replace_with_string_sized(hsl_Context* context, const char* name, const char* value, size_t sz) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || (!value && sz > 0)) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjString* string = manager->CopyString(value, sz);
	if (!string) {
		return HSL_OUT_OF_MEMORY;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, OBJECT_VAL(string), true);
}

hsl_Result hsl_global_replace_with_object(hsl_Context* context, const char* name, hsl_Object* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, OBJECT_VAL(value), true);
}

hsl_Result hsl_global_replace_with_linked_integer(hsl_Context* context, const char* name, int* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, INTEGER_LINK_VAL(value), true);
}

hsl_Result hsl_global_replace_with_linked_decimal(hsl_Context* context, const char* name, float* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Globals, name, DECIMAL_LINK_VAL(value), true);
}

hsl_Result hsl_global_remove(hsl_Context* context, const char* name) {
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

int hsl_constant_exists(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	if (manager->Constants->Exists(name)) {
		return 1;
	}

	return 0;
}

hsl_ValueType hsl_constant_type(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_VAL_INVALID;
	}

	if (!manager->Constants->Exists(name)) {
		return HSL_VAL_INVALID;
	}

	VMValue value = manager->Constants->Get(name);
	return ValueTypeToAPIValueType((ValueType)value.Type);
}

int hsl_constant_as_integer(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return 0;
	}

	if (!manager->Constants->Exists(name)) {
		return 0;
	}

	VMValue value = manager->Constants->Get(name);
	if (IS_INTEGER(value)) {
		return AS_INTEGER(value);
	}

	return 0;
}

float hsl_constant_as_decimal(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return 0.0f;
	}

	if (!manager->Constants->Exists(name)) {
		return 0.0f;
	}

	VMValue value = manager->Constants->Get(name);
	if (IS_DECIMAL(value)) {
		return AS_DECIMAL(value);
	}

	return 0.0f;
}

char* hsl_constant_as_string(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return nullptr;
	}

	if (!manager->Constants->Exists(name)) {
		return nullptr;
	}

	VMValue value = manager->Constants->Get(name);
	if (IS_STRING(value)) {
		return AS_CSTRING(value);
	}

	return nullptr;
}

hsl_Object* hsl_constant_as_object(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return nullptr;
	}

	if (!manager->Constants->Exists(name)) {
		return nullptr;
	}

	VMValue value = manager->Constants->Get(name);
	if (IS_OBJECT(value)) {
		return (hsl_Object*)AS_OBJECT(value);
	}

	return nullptr;
}

hsl_Result hsl_constant_define(hsl_Context* context, const char* name, hsl_Value* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue vmValue = *(VMValue*)value;

	return hsl_table_set_internal(manager, manager->Constants, name, vmValue, true);
}

hsl_Result hsl_constant_define_integer(hsl_Context* context, const char* name, int value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Constants, name, INTEGER_VAL(value), true);
}

hsl_Result hsl_constant_define_decimal(hsl_Context* context, const char* name, float value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Constants, name, DECIMAL_VAL(value), true);
}

hsl_Result hsl_constant_define_string(hsl_Context* context, const char* name, const char* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjString* string = manager->CopyString(value);
	if (!string) {
		return HSL_OUT_OF_MEMORY;
	}

	return hsl_table_set_internal(manager, manager->Constants, name, OBJECT_VAL(string), true);
}

hsl_Result hsl_constant_define_string_sized(hsl_Context* context, const char* name, const char* value, size_t sz) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || (!value && sz > 0)) {
		return HSL_INVALID_ARGUMENT;
	}

	ObjString* string = manager->CopyString(value, sz);
	if (!string) {
		return HSL_OUT_OF_MEMORY;
	}

	return hsl_table_set_internal(manager, manager->Constants, name, OBJECT_VAL(string), true);
}

hsl_Result hsl_constant_define_object(hsl_Context* context, const char* name, hsl_Object* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Constants, name, OBJECT_VAL(value), true);
}

hsl_Result hsl_constant_define_linked_integer(hsl_Context* context, const char* name, int* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Constants, name, INTEGER_LINK_VAL(value), true);
}

hsl_Result hsl_constant_define_linked_decimal(hsl_Context* context, const char* name, float* value) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	return hsl_table_set_internal(manager, manager->Constants, name, DECIMAL_LINK_VAL(value), true);
}

hsl_Result hsl_constant_remove(hsl_Context* context, const char* name) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager || !name) {
		return HSL_INVALID_ARGUMENT;
	}

	if (!manager->Constants->Exists(name)) {
		return HSL_DOES_NOT_EXIST;
	}

	manager->Constants->Remove(name);

	return HSL_OK;
}
#endif
