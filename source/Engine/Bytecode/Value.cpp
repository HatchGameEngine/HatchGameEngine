#include <Engine/Bytecode/Value.h>
#include <Engine/Bytecode/ValuePrinter.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/ResourceTypes/Resource.h>
#include <Engine/ResourceTypes/ResourceType.h>

const char* Value::GetObjectTypeName(Uint32 type) {
	switch (type) {
	case OBJ_FUNCTION:
		return "function";
	case OBJ_BOUND_METHOD:
		return "bound method";
	case OBJ_CLASS:
		return "class";
	case OBJ_CLOSURE:
		return "closure";
	case OBJ_INSTANCE:
		return "instance";
	case OBJ_ENTITY:
		return "entity";
	case OBJ_NATIVE_FUNCTION:
		return "native function";
	case OBJ_NATIVE_INSTANCE:
		return "native instance";
	case OBJ_STRING:
		return "string";
	case OBJ_UPVALUE:
		return "upvalue";
	case OBJ_ARRAY:
		return "array";
	case OBJ_MAP:
		return "map";
	case OBJ_NAMESPACE:
		return "namespace";
	case OBJ_ENUM:
		return "enum";
	case OBJ_MODULE:
		return "module";
	case OBJ_RESOURCE:
		return "resource";
	case OBJ_RESOURCEABLE:
		return "resourceable";
	}

	return "unknown object type";
}

const char* Value::GetObjectTypeName(ObjClass* klass) {
	const char* printableName = TypeImpl::GetPrintableName(klass);
	if (printableName != nullptr) {
		return printableName;
	}
	return "unknown";
}

const char* Value::GetObjectTypeName(VMValue value) {
	Obj* object = AS_OBJECT(value);

	if (object->Type == OBJ_RESOURCE) {
		void* resourcePtr = ((ObjResource*)object)->ResourcePtr;
		ResourceType* resource = (ResourceType*)resourcePtr;
		return GetResourceTypeString(resource->Type);
	}

	const char* printableName = TypeImpl::GetPrintableName(object->Class);
	if (printableName != nullptr) {
		return printableName;
	}

	return GetObjectTypeName(OBJECT_TYPE(value));
}

const char* Value::GetPrintableObjectName(VMValue value) {
	switch (OBJECT_TYPE(value)) {
	case OBJ_CLASS:
		return AS_CLASS(value)->Name ? AS_CLASS(value)->Name->Chars : "(null)";
	case OBJ_BOUND_METHOD:
		return AS_BOUND_METHOD(value)->Method->Name
			? AS_BOUND_METHOD(value)->Method->Name->Chars
			: "(null)";
	case OBJ_CLOSURE:
		return AS_CLOSURE(value)->Function->Name ? AS_CLOSURE(value)->Function->Name->Chars
							 : "(null)";
	case OBJ_FUNCTION:
		return AS_FUNCTION(value)->Name ? AS_FUNCTION(value)->Name->Chars : "(null)";
	case OBJ_MODULE:
		return AS_MODULE(value)->SourceFilename ? AS_MODULE(value)->SourceFilename->Chars
							: "(null)";
	case OBJ_INSTANCE:
	case OBJ_NATIVE_INSTANCE:
	case OBJ_ENTITY:
		return AS_INSTANCE(value)->Object.Class->Name
			? AS_INSTANCE(value)->Object.Class->Name->Chars
			: "(null)";
	case OBJ_NAMESPACE:
		return AS_NAMESPACE(value)->Name ? AS_NAMESPACE(value)->Name->Chars : "(null)";
	case OBJ_STRING:
		return AS_CSTRING(value);
	case OBJ_RESOURCE: {
		ResourceType* resource = (ResourceType*)AS_RESOURCE(value)->ResourcePtr;
		return resource->Filename;
	}
	default:
		return nullptr;
	}
}

std::string Value::ToString(VMValue v) {
	if (IS_STRING(v)) {
		return std::string(AS_CSTRING(v));
	}

	char* buffer = (char*)malloc(512);
	PrintBuffer buffer_info;
	buffer_info.Buffer = &buffer;
	buffer_info.WriteIndex = 0;
	buffer_info.BufferSize = 512;
	ValuePrinter::Print(&buffer_info, v, false);
	std::string result(buffer, buffer_info.WriteIndex);
	free(buffer);
	return result;
}
VMValue Value::CastAsString(VMValue v) {
	if (IS_STRING(v)) {
		return v;
	}

	char* buffer = (char*)malloc(512);
	PrintBuffer buffer_info;
	buffer_info.Buffer = &buffer;
	buffer_info.WriteIndex = 0;
	buffer_info.BufferSize = 512;
	ValuePrinter::Print(&buffer_info, v, false);
	v = OBJECT_VAL(CopyString(buffer, buffer_info.WriteIndex));
	free(buffer);
	return v;
}
VMValue Value::CastAsInteger(VMValue v) {
	float a;
	switch (v.Type) {
	case VAL_DECIMAL:
	case VAL_LINKED_DECIMAL:
		a = AS_DECIMAL(v);
		return INTEGER_VAL((int)a);
	case VAL_INTEGER:
		return v;
	case VAL_LINKED_INTEGER:
		return INTEGER_VAL(AS_INTEGER(v));
	default:
		// NOTE: Should error here instead.
		break;
	}
	return NULL_VAL;
}
VMValue Value::CastAsDecimal(VMValue v) {
	int a;
	switch (v.Type) {
	case VAL_DECIMAL:
		return v;
	case VAL_LINKED_DECIMAL:
		return DECIMAL_VAL(AS_DECIMAL(v));
	case VAL_INTEGER:
	case VAL_LINKED_INTEGER:
		a = AS_INTEGER(v);
		return DECIMAL_VAL((float)a);
	default:
		// NOTE: Should error here instead.
		break;
	}
	return NULL_VAL;
}
VMValue Value::Concatenate(VMValue va, VMValue vb) {
	ObjString* a = AS_STRING(va);
	ObjString* b = AS_STRING(vb);

	size_t length = a->Length + b->Length;
	ObjString* result = AllocString(length);

	memcpy(result->Chars, a->Chars, a->Length);
	memcpy(result->Chars + a->Length, b->Chars, b->Length);
	result->Chars[length] = 0;
	return OBJECT_VAL(result);
}

bool Value::SortaEqual(VMValue a, VMValue b) {
	if ((a.Type == VAL_DECIMAL && b.Type == VAL_INTEGER) ||
		(a.Type == VAL_INTEGER && b.Type == VAL_DECIMAL)) {
		float a_d = AS_DECIMAL(CastAsDecimal(a));
		float b_d = AS_DECIMAL(CastAsDecimal(b));
		return (a_d == b_d);
	}

	if (IS_STRING(a) && IS_STRING(b)) {
		ObjString* astr = AS_STRING(a);
		ObjString* bstr = AS_STRING(b);
		return astr->Length == bstr->Length &&
			!memcmp(astr->Chars, bstr->Chars, astr->Length);
	}

	if (IS_BOUND_METHOD(a) && IS_BOUND_METHOD(b)) {
		ObjBoundMethod* abm = AS_BOUND_METHOD(a);
		ObjBoundMethod* bbm = AS_BOUND_METHOD(b);
		return Value::ExactlyEqual(abm->Receiver, bbm->Receiver) &&
			abm->Method == bbm->Method;
	}

	if (IS_RESOURCE(a) && IS_RESOURCEABLE(b)) {
		return Resource::CompareVMObjects(AS_OBJECT(a), AS_OBJECT(b));
	}
	else if (IS_RESOURCEABLE(a) && IS_RESOURCE(b)) {
		return Resource::CompareVMObjects(AS_OBJECT(b), AS_OBJECT(a));
	}

	return Value::Equal(a, b);
}
bool Value::Equal(VMValue a, VMValue b) {
	if (!(a.Type == VAL_LINKED_INTEGER || a.Type == VAL_LINKED_DECIMAL ||
		    b.Type == VAL_LINKED_INTEGER || b.Type == VAL_LINKED_DECIMAL)) {
		if (a.Type != b.Type) {
			return false;
		}
	}

	switch (a.Type) {
	case VAL_LINKED_INTEGER:
	case VAL_INTEGER:
		return AS_INTEGER(a) == AS_INTEGER(b);
	case VAL_LINKED_DECIMAL:
	case VAL_DECIMAL:
		return AS_DECIMAL(a) == AS_DECIMAL(b);
	case VAL_OBJECT:
		return AS_OBJECT(a) == AS_OBJECT(b);
	case VAL_NULL:
		return true;
	}

	return false;
}
bool Value::ExactlyEqual(VMValue a, VMValue b) {
	if (a.Type != b.Type) {
		return false;
	}

	switch (a.Type) {
	case VAL_INTEGER:
		return AS_INTEGER(a) == AS_INTEGER(b);
	case VAL_DECIMAL:
		return AS_DECIMAL(a) == AS_DECIMAL(b);
	case VAL_OBJECT:
		return AS_OBJECT(a) == AS_OBJECT(b);
	}
	return false;
}
bool Value::Falsey(VMValue a) {
	if (a.Type == VAL_NULL) {
		return true;
	}

	switch (a.Type) {
	case VAL_LINKED_INTEGER:
	case VAL_INTEGER:
		return AS_INTEGER(a) == 0;
	case VAL_LINKED_DECIMAL:
	case VAL_DECIMAL:
		return AS_DECIMAL(a) == 0.0f;
	case VAL_OBJECT:
		return false;
	}
	return false;
}

VMValue Value::Delink(VMValue val) {
	if (IS_LINKED_DECIMAL(val)) {
		return DECIMAL_VAL(AS_DECIMAL(val));
	}
	if (IS_LINKED_INTEGER(val)) {
		return INTEGER_VAL(AS_INTEGER(val));
	}

	return val;
}
