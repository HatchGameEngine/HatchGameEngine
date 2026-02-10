#include <Engine/Diagnostics/Memory.h>
#include <Engine/Types/Property.h>
#include <Engine/Utilities/StringUtils.h>

Property Property::MakeNull() {
	Property val;
	val.Type = PROPERTY_NULL;
	val.as.Integer = 0;
	return val;
}

Property Property::MakeInteger(int value) {
	Property val;
	val.Type = PROPERTY_INTEGER;
	val.as.Integer = value;
	return val;
}

Property Property::MakeDecimal(float value) {
	Property val;
	val.Type = PROPERTY_DECIMAL;
	val.as.Decimal = value;
	return val;
}

Property Property::MakeBool(bool value) {
	Property val;
	val.Type = PROPERTY_BOOL;
	val.as.Bool = value;
	return val;
}

Property Property::MakeString(const char* value) {
	Property val;
	val.Type = PROPERTY_STRING;
	val.as.String = StringUtils::Duplicate(value);
	return val;
}
Property Property::MakeString(const char* value, int length) {
	Property val;
	val.Type = PROPERTY_STRING;
	val.as.String = StringUtils::Create((void*)value, length);
	return val;
}

Property Property::MakeArray(PropertyArray value) {
	Property val;
	val.Type = PROPERTY_ARRAY;
	val.as.Array = value;
	return val;
}

void Property::Delete(Property property) {
	switch (property.Type) {
	case PROPERTY_STRING:
		Memory::Free(property.as.String);
		break;
	case PROPERTY_ARRAY:
		for (size_t i = 0; i < property.as.Array.Count; i++) {
			Property* data = (Property*)property.as.Array.Data;
			Property::Delete(data[i]);
		}
		Memory::Free(property.as.Array.Data);
		break;
	}
}

void PropertyArray::Init(PropertyArray* array) {
	array->Count = 0;
	array->Data = nullptr;
}

void AddPropertyToArray(PropertyArray* array, Property property) {
	Property* data =
		(Property*)Memory::Realloc(array->Data, (array->Count + 1) * sizeof(Property));
	if (!data) {
		return;
	}

	data[array->Count] = property;
	array->Data = data;
	array->Count++;
}
