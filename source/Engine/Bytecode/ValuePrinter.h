#ifndef ENGINE_BYTECODE_VALUEPRINTER_H
#define ENGINE_BYTECODE_VALUEPRINTER_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Utilities/PrintBuffer.h>

class ValuePrinter {
private:
	PrintBuffer* Buffer;
	bool IsJSON;
	bool PrettyPrint;

	void PrintRootValue(VMValue value);

public:
	void PrintValue(VMValue value, int indent);
	void PrintObject(VMValue value, int indent);
	void PrintArray(ObjArray* array, int indent);
	void PrintMap(ObjMap* map, int indent);

	static void Print(VMValue value);
	static void Print(PrintBuffer* buffer, VMValue value);
	static void Print(PrintBuffer* buffer, VMValue value, bool prettyPrint);
	static void Print(PrintBuffer* buffer, VMValue value, bool prettyPrint, bool isJSON);
};

#endif /* ENGINE_BYTECODE_VALUEPRINTER_H */
