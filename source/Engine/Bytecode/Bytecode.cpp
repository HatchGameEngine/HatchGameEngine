#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Utilities/StringUtils.h>

#define BYTECODE_VERSION 0x0005

const char* Bytecode::Magic = "HTVM";
Uint32 Bytecode::LatestVersion = BYTECODE_VERSION;
vector<const char*> Bytecode::FunctionNames{"<anonymous-fn>", "main"};

const char* Bytecode::OpcodeNames[OP_LAST] = {"OP_NOP",
	"OP_CONSTANT",
	"OP_DEFINE_GLOBAL",
	"OP_GET_PROPERTY",
	"OP_SET_PROPERTY",
	"OP_GET_GLOBAL",
	"OP_SET_GLOBAL",
	"OP_GET_LOCAL",
	"OP_SET_LOCAL",
	"OP_PRINT_STACK",
	"OP_INHERIT",
	"OP_RETURN",
	"OP_METHOD_V4",
	"OP_CLASS",
	"OP_CALL",
	"OP_UNUSED_1",
	"OP_INVOKE_V3",
	"OP_JUMP",
	"OP_JUMP_IF_FALSE",
	"OP_JUMP_BACK",
	"OP_POP",
	"OP_COPY",
	"OP_ADD",
	"OP_SUBTRACT",
	"OP_MULTIPLY",
	"OP_DIVIDE",
	"OP_MODULO",
	"OP_NEGATE",
	"OP_INCREMENT",
	"OP_DECREMENT",
	"OP_BITSHIFT_LEFT",
	"OP_BITSHIFT_RIGHT",
	"OP_NULL",
	"OP_TRUE",
	"OP_FALSE",
	"OP_BW_NOT",
	"OP_BW_AND",
	"OP_BW_OR",
	"OP_BW_XOR",
	"OP_LG_NOT",
	"OP_LG_AND",
	"OP_LG_OR",
	"OP_EQUAL",
	"OP_EQUAL_NOT",
	"OP_GREATER",
	"OP_GREATER_EQUAL",
	"OP_LESS",
	"OP_LESS_EQUAL",
	"OP_PRINT",
	"OP_ENUM_NEXT",
	"OP_SAVE_VALUE",
	"OP_LOAD_VALUE",
	"OP_WITH",
	"OP_GET_ELEMENT",
	"OP_SET_ELEMENT",
	"OP_NEW_ARRAY",
	"OP_NEW_MAP",
	"OP_SWITCH_TABLE",
	"OP_UNUSED_2",
	"OP_EVENT_V4",
	"OP_TYPEOF",
	"OP_NEW",
	"OP_IMPORT",
	"OP_SWITCH",
	"OP_POPN",
	"OP_HAS_PROPERTY",
	"OP_IMPORT_MODULE",
	"OP_ADD_ENUM",
	"OP_NEW_ENUM",
	"OP_GET_SUPERCLASS",
	"OP_GET_MODULE_LOCAL",
	"OP_SET_MODULE_LOCAL",
	"OP_DEFINE_MODULE_LOCAL",
	"OP_USE_NAMESPACE",
	"OP_DEFINE_CONSTANT",
	"OP_INTEGER",
	"OP_DECIMAL",
	"OP_INVOKE",
	"OP_SUPER_INVOKE",
	"OP_EVENT",
	"OP_METHOD",
	"OP_NEW_HITBOX"};

Bytecode::Bytecode() {
	Version = LatestVersion;
}

Bytecode::~Bytecode() {
	Memory::Free(SourceFilename);
}

bool Bytecode::Read(BytecodeContainer bytecode, HashMap<char*>* tokens) {
	MemoryStream* stream = MemoryStream::New(bytecode.Data, bytecode.Size);
	if (!stream) {
		return false;
	}

	Uint8 magic[4];
	stream->ReadBytes(magic, 4);
	if (memcmp(Bytecode::Magic, magic, 4) != 0) {
		Log::Print(Log::LOG_ERROR, "Incorrect magic!");
		stream->Close();
		return false;
	}

	Version = stream->ReadByte();

	if (Version > BYTECODE_VERSION) {
		Log::Print(Log::LOG_ERROR, "Unsupported bytecode version 0x%02X!", Version);
		stream->Close();
		return false;
	}

	Flags = stream->ReadByte();
	stream->Skip(1);
	stream->Skip(1);

	HasDebugInfo = Flags & BYTECODE_FLAG_DEBUGINFO;
	bool hasSourceFilename = Flags & BYTECODE_FLAG_SOURCEFILENAME;

	int chunkCount = stream->ReadInt32();
	if (!chunkCount) {
		Log::Print(Log::LOG_ERROR, "Bytecode contains no chunks!");
		stream->Close();
		return false;
	}

	for (int i = 0; i < chunkCount; i++) {
		ObjFunction* function = ReadChunk(stream);

		function->Index = (size_t)i;

		Functions.push_back(function);
	}

	if (HasDebugInfo) {
		int tokenCount = stream->ReadInt32();
		if (tokens) {
			for (int t = 0; t < tokenCount; t++) {
				char* string = stream->ReadString();
				Uint32 hash = Murmur::EncryptString(string);
				if (!tokens->Exists(hash)) {
					tokens->Put(hash, string);
				}
				else {
					Memory::Free(string);
				}
			}

			for (ObjFunction* function : Functions) {
				if (tokens->Exists(function->NameHash)) {
					function->Name = StringUtils::Duplicate(tokens->Get(function->NameHash));
				}
			}
		}
		else {
			for (int t = 0; t < tokenCount; t++) {
				stream->SkipString();
			}
		}
	}
	else {
		char fnHash[9];
		for (ObjFunction* function : Functions) {
			snprintf(fnHash, sizeof(fnHash), "%08X", function->NameHash);
			function->Name = StringUtils::Duplicate(fnHash);
		}
	}

	if (hasSourceFilename) {
		SourceFilename = stream->ReadString();
	}

	stream->Close();
	return true;
}
ObjFunction* Bytecode::ReadChunk(MemoryStream* stream) {
	int length = stream->ReadInt32();
	int arity, minArity;
	int opcodeCount = 0;

	if (Version < 0x0001) {
		arity = stream->ReadInt32();
		minArity = arity;
	}
	else {
		arity = stream->ReadByte();
		minArity = stream->ReadByte();
	}

	if (Version > 0x0002) {
		opcodeCount = stream->ReadUInt32();
	}

	Uint32 hash = stream->ReadUInt32();

	ObjFunction* function = NewFunction();
	function->Arity = arity;
	function->MinArity = minArity;
	function->NameHash = hash;

	Chunk* chunk = &function->Chunk;
	chunk->Count = length;
	chunk->OpcodeCount = opcodeCount;
	chunk->OwnsMemory = false;

	chunk->Code = stream->pointer;
	stream->Skip(length * sizeof(Uint8));

	if (HasDebugInfo) {
		chunk->Lines = (int*)stream->pointer;
		stream->Skip(length * sizeof(int));
	}

	int constantCount = stream->ReadInt32();
	chunk->Constants->reserve(constantCount);
	for (int c = 0; c < constantCount; c++) {
		Uint8 type = stream->ReadByte();
		switch (type) {
		case Bytecode::VALUE_TYPE_INTEGER:
			chunk->AddConstant(INTEGER_VAL(stream->ReadInt32()));
			break;
		case Bytecode::VALUE_TYPE_DECIMAL:
			chunk->AddConstant(DECIMAL_VAL(stream->ReadFloat()));
			break;
		case Bytecode::VALUE_TYPE_HITBOX: {
			Sint16 left = stream->ReadInt16();
			Sint16 top = stream->ReadInt16();
			Sint16 right = stream->ReadInt16();
			Sint16 bottom = stream->ReadInt16();
			chunk->AddConstant(HITBOX_VAL(left, top, right, bottom));
			break;
		}
		case Bytecode::VALUE_TYPE_STRING:
			chunk->AddConstant(
				OBJECT_VAL(TakeString(stream->ReadString())));
			break;
		}
	}

	if (Flags & BYTECODE_FLAG_VARNAMES) {
		Uint16 numLocals = stream->ReadUInt16();
		if (numLocals) {
			chunk->Locals = new vector<ChunkLocal>;
			ReadLocals(stream, chunk->Locals, numLocals);
		}

		Uint16 numModuleLocals = stream->ReadUInt16();
		if (numModuleLocals) {
			chunk->ModuleLocals = new vector<ChunkLocal>;
			ReadLocals(stream, chunk->ModuleLocals, numModuleLocals);
		}
	}

	if (Flags & BYTECODE_FLAG_BREAKPOINTS) {
		size_t numBreakpoints = stream->ReadByte();
		if (numBreakpoints) {
			chunk->Breakpoints = (Uint8*)Memory::Calloc(length, sizeof(Uint8));
			for (size_t i = 0; i < numBreakpoints; i++) {
				Uint32 position = stream->ReadUInt32();
				if (position < chunk->Count) {
					chunk->Breakpoints[position] = 1;
				}
			}
		}
	}

	return function;
}
void Bytecode::ReadLocals(Stream* stream, vector<ChunkLocal>* locals, int numLocals) {
	for (int i = 0; i < numLocals; i++) {
		char* name = stream->ReadString();
		Uint8 flags = stream->ReadByte();

		ChunkLocal local;
		local.Name = name;
		local.Resolved = flags & CHUNKLOCAL_FLAG_RESOLVED;
		local.Constant = flags & CHUNKLOCAL_FLAG_CONST;
		local.Index = stream->ReadUInt32();
		local.Position = stream->ReadUInt32();

		locals->push_back(local);
	}
}

void Bytecode::Write(Stream* stream, HashMap<Token>* tokenMap) {
	size_t chunkCount = Functions.size();

	Flags = 0;

	if (HasDebugInfo) {
		Flags |= BYTECODE_FLAG_DEBUGINFO;
		Flags |= BYTECODE_FLAG_VARNAMES;

		for (size_t c = 0; c < chunkCount; c++) {
			if (Functions[c]->Chunk.Breakpoints) {
				Flags |= BYTECODE_FLAG_BREAKPOINTS;
				break;
			}
		}
	}
	if (SourceFilename) {
		Flags |= BYTECODE_FLAG_SOURCEFILENAME;
	}

	stream->WriteBytes((char*)Bytecode::Magic, 4);
	stream->WriteByte(Version);
	stream->WriteByte(Flags);
	stream->WriteByte(0x00);
	stream->WriteByte(0x00);

	stream->WriteUInt32(chunkCount);

	for (size_t c = 0; c < chunkCount; c++) {
		WriteChunk(stream, Functions[c]);
	}

	// Add tokens
	if (HasDebugInfo && tokenMap) {
		stream->WriteUInt32(tokenMap->Count() + FunctionNames.size());
		std::for_each(
			FunctionNames.begin(), FunctionNames.end(), [stream](const char* name) {
				stream->WriteBytes((void*)name, strlen(name) + 1);
			});
		tokenMap->WithAll([stream](Uint32, Token t) -> void {
			stream->WriteBytes(t.Start, t.Length);
			stream->WriteByte(0); // NULL terminate
		});
	}

	if (SourceFilename) {
		stream->WriteBytes(SourceFilename, strlen(SourceFilename) + 1);
	}
}
void Bytecode::WriteChunk(Stream* stream, ObjFunction* function) {
	Chunk* chunk = &function->Chunk;

	stream->WriteUInt32(chunk->Count);
	if (Version >= 0x0001) {
		stream->WriteByte(function->Arity);
		stream->WriteByte(function->MinArity);
	}
	else {
		stream->WriteUInt32(function->Arity);
	}

	if (Version >= 0x0003) {
		stream->WriteUInt32(chunk->OpcodeCount);
	}

	stream->WriteUInt32(Murmur::EncryptString(function->Name));

	stream->WriteBytes(chunk->Code, chunk->Count);
	if (HasDebugInfo) {
		stream->WriteBytes(chunk->Lines, chunk->Count * sizeof(int));
	}

	int constSize = (int)chunk->Constants->size();
	stream->WriteUInt32(constSize);
	for (int i = 0; i < constSize; i++) {
		VMValue constt = (*chunk->Constants)[i];
		Uint8 type = (Uint8)constt.Type;

		switch (type) {
		case VAL_INTEGER:
			stream->WriteByte(Bytecode::VALUE_TYPE_INTEGER);
			stream->WriteBytes(&AS_INTEGER(constt), sizeof(int));
			break;
		case VAL_DECIMAL:
			stream->WriteByte(Bytecode::VALUE_TYPE_DECIMAL);
			stream->WriteBytes(&AS_DECIMAL(constt), sizeof(float));
			break;
		case VAL_HITBOX:
			stream->WriteByte(Bytecode::VALUE_TYPE_HITBOX);
			stream->WriteInt16(constt.as.Hitbox[HITBOX_LEFT]);
			stream->WriteInt16(constt.as.Hitbox[HITBOX_TOP]);
			stream->WriteInt16(constt.as.Hitbox[HITBOX_RIGHT]);
			stream->WriteInt16(constt.as.Hitbox[HITBOX_BOTTOM]);
			break;
		case VAL_OBJECT:
			if (OBJECT_TYPE(constt) == OBJ_STRING) {
				ObjString* str = AS_STRING(constt);
				stream->WriteByte(Bytecode::VALUE_TYPE_STRING);
				stream->WriteBytes(str->Chars, str->Length + 1);
			}
			else {
				printf("Unsupported object type...Chief. (%s)\n",
					GetObjectTypeString(OBJECT_TYPE(constt)));
				stream->WriteByte(Bytecode::VALUE_TYPE_NULL);
			}
			break;
		}
	}

	if (Flags & BYTECODE_FLAG_VARNAMES) {
		int numLocals = chunk->Locals ? std::min((int)chunk->Locals->size(), 0xFFFF) : 0;
		WriteLocals(stream, chunk->Locals, numLocals);

		numLocals = chunk->ModuleLocals ? std::min((int)chunk->ModuleLocals->size(), 0xFFFF) : 0;
		WriteLocals(stream, chunk->ModuleLocals, numLocals);
	}

	if (Flags & BYTECODE_FLAG_BREAKPOINTS) {
		if (chunk->Breakpoints) {
			std::vector<Uint32> breakpoints;
			for (size_t i = 0; i < chunk->Count; i++) {
				if (chunk->Breakpoints[i]) {
					breakpoints.push_back(i);
				}
			}

			stream->WriteByte(breakpoints.size());
			for (size_t i = 0; i < breakpoints.size(); i++) {
				stream->WriteUInt32(breakpoints[i]);
			}
		}
		else {
			stream->WriteByte(0);
		}
	}
}
void Bytecode::WriteLocals(Stream* stream, vector<ChunkLocal>* locals, int numLocals) {
	if (!locals) {
		stream->WriteUInt16(0);
		return;
	}

	stream->WriteUInt16(numLocals);

	for (int i = 0; i < numLocals; i++) {
		ChunkLocal local = (*locals)[i];

		Uint8 flags = 0;
		if (local.Resolved) {
			flags |= CHUNKLOCAL_FLAG_RESOLVED;
		}
		if (local.Constant) {
			flags |= CHUNKLOCAL_FLAG_CONST;
		}

		stream->WriteBytes((void*)local.Name, strlen(local.Name) + 1);
		stream->WriteByte(flags);
		stream->WriteUInt32(local.Index);
		stream->WriteUInt32(local.Position);
	}
}

int Bytecode::GetTotalOpcodeSize(uint8_t* op) {
	switch (*op) {
		// ConstantInstruction
	case OP_CONSTANT:
	case OP_INTEGER:
	case OP_DECIMAL:
	case OP_IMPORT:
	case OP_IMPORT_MODULE:
		return 5;
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
		return 1;
	case OP_COPY:
	case OP_CALL:
	case OP_NEW:
	case OP_EVENT_V4:
	case OP_POPN:
		return 2;
	case OP_EVENT:
		return 3;
	case OP_GET_LOCAL:
	case OP_SET_LOCAL:
		return 2;
	case OP_GET_GLOBAL:
	case OP_DEFINE_GLOBAL:
	case OP_DEFINE_CONSTANT:
	case OP_SET_GLOBAL:
	case OP_GET_PROPERTY:
	case OP_SET_PROPERTY:
	case OP_HAS_PROPERTY:
	case OP_USE_NAMESPACE:
	case OP_INHERIT:
		return 5;
	case OP_SET_MODULE_LOCAL:
	case OP_GET_MODULE_LOCAL:
		return 3;
	case OP_NEW_ARRAY:
	case OP_NEW_MAP:
		return 5;
	case OP_JUMP:
	case OP_JUMP_IF_FALSE:
	case OP_JUMP_BACK:
		return 3;
	case OP_INVOKE:
	case OP_SUPER_INVOKE:
		return 6;
	case OP_INVOKE_V3:
		return 7;
	case OP_WITH:
		if (*(op + 1) == 3) {
			return 5;
		}
		return 4;
	case OP_CLASS:
		return 6;
	case OP_ADD_ENUM:
	case OP_NEW_ENUM:
		return 5;
	case OP_METHOD:
		return 7;
	case OP_METHOD_V4:
		return 6;
	}
	return 1;
}
