#ifndef ENGINE_BYTECODE_VALUES_H
#define ENGINE_BYTECODE_VALUES_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Utilities/PrintBuffer.h>

class Value {
public:
	static const char* GetPrintableObjectName(VMValue value);
	static const char* GetObjectTypeName(VMValue value);

	static VMValue CastAsString(VMValue v);
	static VMValue CastAsInteger(VMValue v);
	static VMValue CastAsDecimal(VMValue v);
	static VMValue Concatenate(VMValue va, VMValue vb);
	static bool SortaEqual(VMValue a, VMValue b);
	static bool Equal(VMValue a, VMValue b);
	static bool Falsey(VMValue a);
	static VMValue Delink(VMValue val);
};

#endif /* ENGINE_BYTECODE_VALUES_H */
