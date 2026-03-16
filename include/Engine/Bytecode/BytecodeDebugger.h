#ifndef ENGINE_BYTECODE_BYTECODEDEBUGGER_H
#define ENGINE_BYTECODE_BYTECODEDEBUGGER_H

#include <Engine/Bytecode/CompilerEnums.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/HashMap.h>

class BytecodeDebugger {
public:
	bool PrintToLog = false;
	HashMap<char*>* Tokens = nullptr;

	int HashInstruction(uint8_t opcode, Chunk* chunk, int offset);
	int ConstantInstruction(uint8_t opcode, Chunk* chunk, int offset);
	int SimpleInstruction(uint8_t opcode, Chunk* chunk, int offset);
	int ByteInstruction(uint8_t opcode, Chunk* chunk, int offset);
	int ShortInstruction(uint8_t opcode, Chunk* chunk, int offset);
	int LocalInstruction(uint8_t opcode, Chunk* chunk, int offset);
	int MethodInstruction(uint8_t opcode, Chunk* chunk, int offset);
	int MethodInstructionV4(uint8_t opcode, Chunk* chunk, int offset);
	int InvokeInstruction(uint8_t opcode, Chunk* chunk, int offset);
	int InvokeInstructionV3(uint8_t opcode, Chunk* chunk, int offset);
	int JumpInstruction(uint8_t opcode, int sign, Chunk* chunk, int offset);
	int ClassInstruction(uint8_t opcode, Chunk* chunk, int offset);
	int EnumInstruction(uint8_t opcode, Chunk* chunk, int offset);
	int WithInstruction(uint8_t opcode, Chunk* chunk, int offset);
	int DebugInstruction(Chunk* chunk, int offset);
	void DebugChunk(Chunk* chunk, const char* name, int minArity, int maxArity);
	~BytecodeDebugger();
};

#endif /* ENGINE_BYTECODE_BYTECODEDEBUGGER_H */
