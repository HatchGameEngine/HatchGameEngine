#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Exceptions/ScriptException.h>

hsl_Object* hsl_object_get_class(hsl_Object* object) {
	if (!object) {
		return nullptr;
	}

	return (hsl_Object*)((Obj*)object)->Class;
}

hsl_Result hsl_call_getter_internal(VMThread* thread, Obj* object, ValueGetFn getter, Uint32 hash, VMValue* result) {
	try {
		if (getter(object, hash, result, thread)) {
			return HSL_OK;
		}
	} catch (const ScriptException& error) {
		return HSL_RUNTIME_ERROR;
	}

	return HSL_DOES_NOT_EXIST;
}

hsl_Result hsl_get_field_internal(VMThread* thread, VMValue object, Uint32 hash, bool callGetter, VMValue& result) {
	VMValue value;

	hsl_Result callResult = HSL_DOES_NOT_EXIST;

	if (IS_INSTANCEABLE(object)) {
		ObjInstance* instance = AS_INSTANCE(object);
		if (instance->Fields->GetIfExists(hash, &value)) {
			result = Value::Delink(value);
			callResult = HSL_OK;
		}
		else if (callGetter) {
			callResult = hsl_call_getter_internal(thread, AS_OBJECT(object), instance->PropertyGet, hash, &value);
			if (callResult == HSL_OK) {
				result = value;
			}
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
		callResult = hsl_call_getter_internal(thread, objPtr, objPtr->Class->PropertyGet, hash, &value);
		if (callResult == HSL_OK) {
			result = value;
		}
	}
	else {
		callResult = HSL_INVALID_ARGUMENT;
	}

	return callResult;
}

hsl_Result hsl_field_exists_internal(VMThread* thread, VMValue object, Uint32 hash, bool callGetter) {
	hsl_Result callResult = HSL_DOES_NOT_EXIST;

	if (IS_INSTANCEABLE(object)) {
		ObjInstance* instance = AS_INSTANCE(object);
		if (instance->Fields->Exists(hash)) {
			callResult = HSL_OK;
		}
		else if (callGetter) {
			callResult = hsl_call_getter_internal(thread, AS_OBJECT(object), instance->PropertyGet, hash, nullptr);
		}
	}
	else if (IS_CLASS(object)) {
		ObjClass* klass = AS_CLASS(object);
		if (klass->Fields->Exists(hash)) {
			callResult = HSL_OK;
		}
	}
	else if (IS_NAMESPACE(object)) {
		ObjNamespace* ns = AS_NAMESPACE(object);
		if (ns->Fields->Exists(hash)) {
			callResult = HSL_OK;
		}
	}
	else if (callGetter && IS_OBJECT(object) && AS_OBJECT(object)->Class) {
		Obj* objPtr = AS_OBJECT(object);
		callResult = hsl_call_getter_internal(thread, objPtr, objPtr->Class->PropertyGet, hash, nullptr);
	}
	else {
		callResult = HSL_INVALID_ARGUMENT;
	}

	return callResult;
}

int hsl_field_exists(hsl_Object* object, const char* name, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return 0;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	hsl_Result result = hsl_field_exists_internal(vmThread, OBJECT_VAL(object), hash, true);
	if (result != HSL_OK) {
		return 0;
	}

	return 1;
}

int hsl_field_as_integer(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

float hsl_field_as_decimal(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

char* hsl_field_as_string(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

hsl_Object* hsl_field_as_object(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

hsl_Result hsl_field_push_to_stack(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

hsl_ValueType hsl_field_type(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

int hsl_field_exists_direct(hsl_Object* object, const char* name, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !name) {
		return 0;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	hsl_Result result = hsl_field_exists_internal(vmThread, OBJECT_VAL(object), hash, false);
	if (result != HSL_OK) {
		return 0;
	}

	return 1;
}

int hsl_field_as_integer_direct(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

float hsl_field_as_decimal_direct(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

char* hsl_field_as_string_direct(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

hsl_Object* hsl_field_as_object_direct(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

hsl_Result hsl_field_push_to_stack_direct(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

hsl_ValueType hsl_field_type_direct(hsl_Object* object, const char* name, hsl_Thread* thread) {
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

hsl_Result hsl_field_set_internal(VMThread* thread, VMValue object, Uint32 hash, bool callSetter, VMValue value) {
	Table* fields = nullptr;
	ValueSetFn setter = nullptr;

	if (IS_INSTANCEABLE(object)) {
		ObjInstance* instance = AS_INSTANCE(object);
		ObjClass* klass = instance->Object.Class;
		fields = instance->Fields;
		if (callSetter) {
			setter = instance->PropertySet;
		}
	}
	else if (IS_CLASS(object)) {
		ObjClass* klass = AS_CLASS(object);
		fields = klass->Fields;
	}
	else if (IS_OBJECT(object) && AS_OBJECT(object)->Class) {
		Obj* objPtr = AS_OBJECT(object);
		ObjClass* klass = objPtr->Class;
		fields = klass->Fields;
		if (callSetter) {
			setter = klass->PropertySet;
		}
	}
	else {
		return HSL_INVALID_ARGUMENT;
	}

	VMValue field;
	if (fields->GetIfExists(hash, &field)) {
		if (!callSetter) {
			fields->Put(hash, value);
		}
		else {
			VMThread converted;
			switch (field.Type) {
			case VAL_LINKED_INTEGER:
				value = Value::CastAsInteger(value);
				if (IS_NULL(value)) {
					return HSL_INVALID_ARGUMENT;
				}
				AS_LINKED_INTEGER(field) = AS_INTEGER(value);
				break;
			case VAL_LINKED_DECIMAL:
				value = Value::CastAsDecimal(value);
				if (IS_NULL(value)) {
					return HSL_INVALID_ARGUMENT;
				}
				AS_LINKED_DECIMAL(field) = AS_DECIMAL(value);
				break;
			default:
				fields->Put(hash, value);
				break;
			}
		}
	}
	else {
		if (setter && setter(AS_OBJECT(object), hash, value, thread)) {
			return HSL_OK;
		}

		fields->Put(hash, value);
	}

	return HSL_OK;
}

hsl_Result hsl_field_set(hsl_Object* object, const char* name, hsl_Value* value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue vmValue = *(VMValue*)value;

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, true, vmValue);
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_set_integer(hsl_Object* object, const char* name, int value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue vmValue = INTEGER_VAL(value);

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, true, vmValue);
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_set_decimal(hsl_Object* object, const char* name, float value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue vmValue = DECIMAL_VAL(value);

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, true, vmValue);
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_set_string(hsl_Object* object, const char* name, const char* value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	ObjString* string = vmThread->Manager->CopyString(value);
	if (!string) {
		return HSL_OUT_OF_MEMORY;
	}

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, true, OBJECT_VAL(string));
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_set_string_sized(hsl_Object* object, const char* name, const char* value, size_t sz, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || (!value && sz > 0)) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	ObjString* string = vmThread->Manager->CopyString(value, sz);
	if (!string) {
		return HSL_OUT_OF_MEMORY;
	}

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, true, OBJECT_VAL(string));
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_set_object(hsl_Object* object, const char* name, hsl_Object* value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue vmValue = OBJECT_VAL(value);

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, true, vmValue);
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_replace(hsl_Object* object, const char* name, hsl_Value* value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue vmValue = *(VMValue*)value;

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, false, vmValue);
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_replace_with_integer(hsl_Object* object, const char* name, int value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue vmValue = INTEGER_VAL(value);

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, false, vmValue);
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_replace_with_decimal(hsl_Object* object, const char* name, float value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue vmValue = DECIMAL_VAL(value);

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, false, vmValue);
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_replace_with_string(hsl_Object* object, const char* name, const char* value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	ObjString* string = vmThread->Manager->CopyString(value);
	if (!string) {
		return HSL_OUT_OF_MEMORY;
	}

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, false, OBJECT_VAL(string));
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_replace_with_string_sized(hsl_Object* object, const char* name, const char* value, size_t sz, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || (!value && sz > 0)) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	ObjString* string = vmThread->Manager->CopyString(value, sz);
	if (!string) {
		return HSL_OUT_OF_MEMORY;
	}

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, false, OBJECT_VAL(string));
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_replace_with_object(hsl_Object* object, const char* name, hsl_Object* value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue vmValue = OBJECT_VAL(value);

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, false, vmValue);
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_replace_with_linked_integer(hsl_Object* object, const char* name, int* value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue vmValue = INTEGER_LINK_VAL(value);

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, false, vmValue);
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_field_replace_with_linked_decimal(hsl_Object* object, const char* name, float* value, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object || !value) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	VMValue vmValue = DECIMAL_LINK_VAL(value);

	hsl_Result result = hsl_field_set_internal(vmThread, OBJECT_VAL(object), hash, false, vmValue);
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}

hsl_Result hsl_remove_field_internal(VMThread* thread, VMValue object, Uint32 hash) {
	Table* fields = nullptr;

	if (IS_INSTANCEABLE(object)) {
		ObjInstance* instance = AS_INSTANCE(object);
		ObjClass* klass = instance->Object.Class;
		fields = instance->Fields;
	}
	else if (IS_CLASS(object)) {
		ObjClass* klass = AS_CLASS(object);
		fields = klass->Fields;
	}
	else if (IS_OBJECT(object) && AS_OBJECT(object)->Class) {
		Obj* objPtr = AS_OBJECT(object);
		ObjClass* klass = objPtr->Class;
		fields = klass->Fields;
	}
	else {
		return HSL_INVALID_ARGUMENT;
	}

	if (!fields->Exists(hash)) {
		return HSL_DOES_NOT_EXIST;
	}

	fields->Remove(hash);

	return HSL_OK;
}

hsl_Result hsl_remove_field(hsl_Object* object, const char* name, hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread || !object) {
		return HSL_INVALID_ARGUMENT;
	}

	Uint32 hash = hsl_get_hash_internal(name);

	hsl_Result result = hsl_remove_field_internal(vmThread, OBJECT_VAL(object), hash);
	if (result != HSL_OK) {
		return result;
	}

	return HSL_OK;
}
#endif
