#ifndef ENGINE_BYTECODE_BYTECODE_H
#define ENGINE_BYTECODE_BYTECODE_H

#include <Engine/Bytecode/CompilerEnums.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/IO/MemoryStream.h>

#define BYTECODE_FLAG_DEBUGINFO (1 << 0)
#define BYTECODE_FLAG_SOURCEFILENAME (1 << 1)
#define BYTECODE_FLAG_VARNAMES (1 << 2)

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
	Uint8 Flags;
	bool HasDebugInfo = false;
	char* SourceFilename = nullptr;

	Bytecode();
	~Bytecode();
	bool Read(BytecodeContainer bytecode, HashMap<char*>* tokens);
	void ReadChunk(MemoryStream* stream);
	void ReadLocals(Stream* stream, vector<ChunkLocal>* locals, int numLocals);
	void Write(Stream* stream, HashMap<Token>* tokenMap);
	void WriteChunk(Stream* stream, ObjFunction* function);
	void WriteLocals(Stream* stream, vector<ChunkLocal>* locals, int numLocals);

	static int GetTotalOpcodeSize(uint8_t* op);
};

#endif /* ENGINE_BYTECODE_BYTECODE_H */
