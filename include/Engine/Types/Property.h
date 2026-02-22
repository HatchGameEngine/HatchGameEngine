#ifndef ENGINE_TYPES_PROPERTY_H
#define ENGINE_TYPES_PROPERTY_H

#include <Engine/Includes/Standard.h>

enum {
	PROPERTY_NULL,
	PROPERTY_INTEGER,
	PROPERTY_DECIMAL,
	PROPERTY_BOOL,
	PROPERTY_STRING,
	PROPERTY_ARRAY
};

struct PropertyArray {
	void* Data;
	size_t Count;

	static void Init(PropertyArray* array);
};

struct Property {
	Uint8 Type;
	union {
		int Integer;
		float Decimal;
		bool Bool;
		char* String;
		PropertyArray Array;
	} as;

	static Property MakeNull();
	static Property MakeInteger(int value);
	static Property MakeDecimal(float value);
	static Property MakeBool(bool value);
	static Property MakeString(const char* value);
	static Property MakeString(const char* value, int length);
	static Property MakeArray(PropertyArray value);
	static void Delete(Property value);
};

void AddPropertyToArray(PropertyArray* array, Property value);

#endif /* ENGINE_TYPES_PROPERTY_H */
