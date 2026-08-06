#ifndef ENGINE_PLATFORMS_CAPABILITY_H
#define ENGINE_PLATFORMS_CAPABILITY_H

class Capability {
public:
	Capability() { Type = TYPE_NULL; }
	Capability(int value) {
		Type = TYPE_INTEGER;
		AsInteger = value;
	}
	Capability(float value) {
		Type = TYPE_DECIMAL;
		AsDecimal = value;
	}
	Capability(bool value) {
		Type = TYPE_BOOL;
		AsBoolean = value;
	}
	Capability(char* value) {
		Type = TYPE_STRING;
		AsString = value;
	};

	enum { TYPE_NULL, TYPE_INTEGER, TYPE_DECIMAL, TYPE_BOOL, TYPE_STRING };

	Uint8 Type;

	union {
		int AsInteger;
		float AsDecimal;
		bool AsBoolean;
		char* AsString;
	};

	void Dispose() {
		if (Type == Capability::TYPE_STRING) {
			Memory::Free(AsString);
		}
	}
};

#endif /* ENGINE_PLATFORMS_CAPABILITY_H */
