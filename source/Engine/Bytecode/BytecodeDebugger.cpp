#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/BytecodeDebugger.h>
#include <Engine/Bytecode/ValuePrinter.h>
#include <Engine/Diagnostics/Log.h>

#define DEBUGGER_LOG(...) \
	if (PrintToLog) { \
		Log::PrintSimple(__VA_ARGS__); \
	} \
	else { \
		printf(__VA_ARGS__); \
	}

int BytecodeDebugger::HashInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	uint32_t hash = *(uint32_t*)&chunk->Code[offset + 1];
	DEBUGGER_LOG("%-16s #%08X", Bytecode::OpcodeNames[opcode], hash);
	if (Tokens && Tokens->Exists(hash)) {
		char* t = Tokens->Get(hash);
		DEBUGGER_LOG(" (%s)", t);
	}
	DEBUGGER_LOG("\n");
	return offset + Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
}
int BytecodeDebugger::ConstantInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	int constant;
	VMValue value;
	if (chunk->GetConstant(offset, &value, &constant)) {
		if (constant != -1) {
			DEBUGGER_LOG("%-16s %9d '", Bytecode::OpcodeNames[opcode], constant);
		}
		else {
			DEBUGGER_LOG("%-16s           '", Bytecode::OpcodeNames[opcode]);
		}
	}
	else {
		constant = *(int*)&chunk->Code[offset + 1];
		value = (*chunk->Constants)[constant];
		DEBUGGER_LOG("%-16s %9d '", Bytecode::OpcodeNames[opcode], constant);
	}
	ValuePrinter::Print(value);
	DEBUGGER_LOG("'\n");
	return offset + Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
}
int BytecodeDebugger::SimpleInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	DEBUGGER_LOG("%s\n", Bytecode::OpcodeNames[opcode]);
	return offset + Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
}
int BytecodeDebugger::ByteInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	DEBUGGER_LOG("%-16s %9d\n", Bytecode::OpcodeNames[opcode], chunk->Code[offset + 1]);
	return offset + Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
}
int BytecodeDebugger::ShortInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	uint16_t data = (uint16_t)(chunk->Code[offset + 1]);
	data |= chunk->Code[offset + 2] << 8;
	DEBUGGER_LOG("%-16s %9d\n", Bytecode::OpcodeNames[opcode], data);
	return offset + Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
}
int BytecodeDebugger::LocalInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	uint8_t slot = chunk->Code[offset + 1];
	if (slot > 0) {
		DEBUGGER_LOG("%-16s %9d\n", Bytecode::OpcodeNames[opcode], slot);
	}
	else {
		DEBUGGER_LOG("%-16s %9d 'this'\n", Bytecode::OpcodeNames[opcode], slot);
	}
	return offset + Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
}
int BytecodeDebugger::MethodInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	uint16_t slot = (uint16_t)(chunk->Code[offset + 1]);
	uint32_t hash = *(uint32_t*)&chunk->Code[offset + 3];
	DEBUGGER_LOG("%-13s %2d", Bytecode::OpcodeNames[opcode], slot);
	DEBUGGER_LOG(" #%08X", hash);
	if (Tokens && Tokens->Exists(hash)) {
		char* t = Tokens->Get(hash);
		DEBUGGER_LOG(" (%s)", t);
	}
	DEBUGGER_LOG("\n");
	return offset + Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
}
int BytecodeDebugger::InvokeInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	return BytecodeDebugger::MethodInstruction(opcode, chunk, offset);
}
int BytecodeDebugger::MethodInstructionV4(uint8_t opcode, Chunk* chunk, int offset) {
	uint8_t slot = chunk->Code[offset + 1];
	uint32_t hash = *(uint32_t*)&chunk->Code[offset + 2];
	DEBUGGER_LOG("%-13s %2d", Bytecode::OpcodeNames[opcode], slot);
	DEBUGGER_LOG(" #%08X", hash);
	if (Tokens && Tokens->Exists(hash)) {
		char* t = Tokens->Get(hash);
		DEBUGGER_LOG(" (%s)", t);
	}
	DEBUGGER_LOG("\n");
	return offset + Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
}
int BytecodeDebugger::InvokeInstructionV3(uint8_t opcode, Chunk* chunk, int offset) {
	return BytecodeDebugger::MethodInstructionV4(opcode, chunk, offset);
}
int BytecodeDebugger::JumpInstruction(uint8_t opcode, int sign, Chunk* chunk, int offset) {
	uint16_t jump = (uint16_t)(chunk->Code[offset + 1]);
	jump |= chunk->Code[offset + 2] << 8;
	DEBUGGER_LOG(
		"%-16s %9d -> %d\n", Bytecode::OpcodeNames[opcode], offset, offset + 3 + sign * jump);
	return offset + Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
}
int BytecodeDebugger::ClassInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	return BytecodeDebugger::HashInstruction(opcode, chunk, offset);
}
int BytecodeDebugger::EnumInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	return BytecodeDebugger::HashInstruction(opcode, chunk, offset);
}
int BytecodeDebugger::WithInstruction(uint8_t opcode, Chunk* chunk, int offset) {
	uint8_t type = chunk->Code[offset + 1];
	uint8_t slot = 0;
	if (type == 3) {
		slot = chunk->Code[offset + 2];
		offset++;
	}
	uint16_t jump = (uint16_t)(chunk->Code[offset + 2]);
	jump |= chunk->Code[offset + 3] << 8;
	if (slot > 0) {
		DEBUGGER_LOG("%-16s %1d %7d -> %d\n", Bytecode::OpcodeNames[opcode], type, slot, jump);
	}
	else {
		DEBUGGER_LOG(
			"%-16s %1d %7d 'this' -> %d\n", Bytecode::OpcodeNames[opcode], type, slot, jump);
	}
	if (type == 3) {
		offset--;
	}
	return offset + Bytecode::GetTotalOpcodeSize(chunk->Code + offset);
}
int BytecodeDebugger::DebugInstruction(Chunk* chunk, int offset) {
	DEBUGGER_LOG("%04d ", offset);
	if (offset > 0 && (chunk->Lines[offset] & 0xFFFF) == (chunk->Lines[offset - 1] & 0xFFFF)) {
		DEBUGGER_LOG("   | ");
	}
	else {
		DEBUGGER_LOG("%4d ", chunk->Lines[offset] & 0xFFFF);
	}

	uint8_t instruction = chunk->Code[offset];
	switch (instruction) {
	case OP_CONSTANT:
	case OP_INTEGER:
	case OP_DECIMAL:
	case OP_IMPORT:
	case OP_IMPORT_MODULE:
		return ConstantInstruction(instruction, chunk, offset);
	case OP_NULL:
	case OP_TRUE:
	case OP_FALSE:
	case OP_POP:
	case OP_INCREMENT:
	case OP_DECREMENT:
	case OP_BITSHIFT_LEFT:
	case OP_BITSHIFT_RIGHT:
	case OP_EQUAL:
	case OP_EQUAL_NOT:
	case OP_LESS:
	case OP_LESS_EQUAL:
	case OP_GREATER:
	case OP_GREATER_EQUAL:
	case OP_ADD:
	case OP_SUBTRACT:
	case OP_MULTIPLY:
	case OP_MODULO:
	case OP_DIVIDE:
	case OP_BW_NOT:
	case OP_BW_AND:
	case OP_BW_OR:
	case OP_BW_XOR:
	case OP_LG_NOT:
	case OP_LG_AND:
	case OP_LG_OR:
	case OP_GET_ELEMENT:
	case OP_SET_ELEMENT:
	case OP_NEGATE:
	case OP_PRINT:
	case OP_TYPEOF:
	case OP_RETURN:
	case OP_SAVE_VALUE:
	case OP_LOAD_VALUE:
	case OP_GET_SUPERCLASS:
	case OP_DEFINE_MODULE_LOCAL:
	case OP_ENUM_NEXT:
	case OP_NEW_HITBOX:
	case OP_UNUSED_1:
		return SimpleInstruction(instruction, chunk, offset);
	case OP_COPY:
	case OP_CALL:
	case OP_NEW:
	case OP_EVENT_V4:
	case OP_POPN:
		return ByteInstruction(instruction, chunk, offset);
	case OP_EVENT:
		return ShortInstruction(instruction, chunk, offset);
	case OP_GET_LOCAL:
	case OP_SET_LOCAL:
		return LocalInstruction(instruction, chunk, offset);
	case OP_GET_GLOBAL:
	case OP_DEFINE_GLOBAL:
	case OP_DEFINE_CONSTANT:
	case OP_SET_GLOBAL:
	case OP_GET_PROPERTY:
	case OP_SET_PROPERTY:
	case OP_HAS_PROPERTY:
	case OP_USE_NAMESPACE:
	case OP_INHERIT:
		return HashInstruction(instruction, chunk, offset);
	case OP_SET_MODULE_LOCAL:
	case OP_GET_MODULE_LOCAL:
		return ShortInstruction(instruction, chunk, offset);
	case OP_NEW_ARRAY:
	case OP_NEW_MAP:
		return SimpleInstruction(instruction, chunk, offset);
	case OP_JUMP:
	case OP_JUMP_IF_FALSE:
		return JumpInstruction(instruction, 1, chunk, offset);
	case OP_JUMP_BACK:
		return JumpInstruction(instruction, -1, chunk, offset);
	case OP_INVOKE:
	case OP_SUPER_INVOKE:
		return InvokeInstruction(instruction, chunk, offset);
	case OP_INVOKE_V3:
		return InvokeInstructionV3(instruction, chunk, offset);

	case OP_PRINT_STACK: {
		offset++;
		uint8_t constant = chunk->Code[offset++];
		DEBUGGER_LOG("%-16s %4d ", Bytecode::OpcodeNames[instruction], constant);
		ValuePrinter::Print((*chunk->Constants)[constant]);
		DEBUGGER_LOG("\n");

		ObjFunction* function = AS_FUNCTION((*chunk->Constants)[constant]);
		for (int j = 0; j < function->UpvalueCount; j++) {
			int isLocal = chunk->Code[offset++];
			int index = chunk->Code[offset++];
			DEBUGGER_LOG("%04d   |                     %s %d\n",
				offset - 2,
				isLocal ? "local" : "upvalue",
				index);
		}

		return offset;
	}
	case OP_WITH:
		return WithInstruction(instruction, chunk, offset);
	case OP_CLASS:
		return ClassInstruction(instruction, chunk, offset);
	case OP_ADD_ENUM:
	case OP_NEW_ENUM:
		return EnumInstruction(instruction, chunk, offset);
	case OP_METHOD:
		return MethodInstruction(instruction, chunk, offset);
	case OP_METHOD_V4:
		return MethodInstructionV4(instruction, chunk, offset);
	default:
		if (instruction < OP_LAST) {
			DEBUGGER_LOG("No viewer for opcode %s\n", Bytecode::OpcodeNames[instruction]);
		}
		else {
			DEBUGGER_LOG("Unknown opcode %d\n", instruction);
		}
		return chunk->Count + 1;
	}
}
void BytecodeDebugger::DebugChunk(Chunk* chunk, const char* name, int minArity, int maxArity) {
	int optArgCount = maxArity - minArity;
	if (optArgCount) {
		DEBUGGER_LOG(
			"== %s (argCount: %d, optArgCount: %d) ==\n", name, maxArity, optArgCount);
	}
	else {
		DEBUGGER_LOG("== %s (argCount: %d) ==\n", name, maxArity);
	}
	DEBUGGER_LOG("byte   ln\n");
	for (int offset = 0; offset < chunk->Count;) {
		offset = DebugInstruction(chunk, offset);
	}

	DEBUGGER_LOG("\nConstants: (%d count)\n", (int)(*chunk->Constants).size());
	for (size_t i = 0; i < (*chunk->Constants).size(); i++) {
		DEBUGGER_LOG(" %2d '", (int)i);
		ValuePrinter::Print((*chunk->Constants)[i]);
		DEBUGGER_LOG("'\n");
	}
}

BytecodeDebugger::~BytecodeDebugger() {
	if (Tokens) {
		Tokens->WithAll([](Uint32 hash, char* token) -> void {
			Memory::Free(token);
		});
		Tokens->Clear();
		delete Tokens;
	}
}
