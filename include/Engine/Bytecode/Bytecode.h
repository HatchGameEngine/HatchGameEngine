#ifndef ENGINE_BYTECODE_BYTECODE_H
#define ENGINE_BYTECODE_BYTECODE_H

#include <Engine/Bytecode/CompilerEnums.h>
#include <Engine/Bytecode/Types.h>

class Bytecode {
private:
	enum {
		VALUE_TYPE_NULL,
		VALUE_TYPE_INTEGER,
		VALUE_TYPE_DECIMAL,
		VALUE_TYPE_STRING,
		VALUE_TYPE_HITBOX
	};

public:
	static const char* Magic;
	static Uint32 LatestVersion;
	static vector<const char*> FunctionNames;
	static const char* OpcodeNames[OP_LAST];

	vector<ObjFunction*> Functions;
	Uint8 Version;
	bool HasDebugInfo = false;
	const char* SourceFilename = nullptr;

	Bytecode();
	~Bytecode();
	bool Read(BytecodeContainer bytecode, HashMap<char*>* tokens);
	void Write(Stream* stream, const char* sourceFilename, HashMap<Token>* tokenMap);

	static int GetTotalOpcodeSize(uint8_t* op);
};

#endif /* ENGINE_BYTECODE_BYTECODE_H */
