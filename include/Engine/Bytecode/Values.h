#ifndef ENGINE_BYTECODE_VALUES_H
#define ENGINE_BYTECODE_VALUES_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/PrintBuffer.h>
#include <Engine/Includes/Standard.h>

class Values {
public:
	static void PrintValue(VMValue value);
	static void PrintValue(PrintBuffer* buffer, VMValue value);
	static void PrintValue(PrintBuffer* buffer, VMValue value, bool prettyPrint);
	static void PrintValue(PrintBuffer* buffer, VMValue value, int indent, bool prettyPrint);
	static void PrintObject(PrintBuffer* buffer, VMValue value, int indent, bool prettyPrint);
};

#endif /* ENGINE_BYTECODE_VALUES_H */
