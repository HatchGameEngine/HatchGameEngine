#include <Engine/Bytecode/Bytecode.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/Utilities/StringUtils.h>

#define BYTECODE_VERSION 0x0003

const char* Bytecode::Magic = "HTVM";
Uint32 Bytecode::LatestVersion = BYTECODE_VERSION;
vector<const char*> Bytecode::FunctionNames{"<anonymous-fn>", "main"};

Bytecode::Bytecode() {
	Version = LatestVersion;
}

Bytecode::~Bytecode() {
	Memory::Free((void*)SourceFilename);
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

	Uint8 opts = stream->ReadByte();
	stream->Skip(1);
	stream->Skip(1);

	HasDebugInfo = opts & 1;

	int chunkCount = stream->ReadInt32();
	if (!chunkCount) {
		return false;
	}

	for (int i = 0; i < chunkCount; i++) {
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
		function->Chunk.Count = length;
		function->Chunk.OpcodeCount = opcodeCount;
		function->Chunk.OwnsMemory = false;

		function->Chunk.Code = stream->pointer;
		stream->Skip(length * sizeof(Uint8));

		if (HasDebugInfo) {
			function->Chunk.Lines = (int*)stream->pointer;
			stream->Skip(length * sizeof(int));
		}

		int constantCount = stream->ReadInt32();
		function->Chunk.Constants->reserve(constantCount);
		for (int c = 0; c < constantCount; c++) {
			Uint8 type = stream->ReadByte();
			switch (type) {
			case VAL_INTEGER:
				function->Chunk.AddConstant(INTEGER_VAL(stream->ReadInt32()));
				break;
			case VAL_DECIMAL:
				function->Chunk.AddConstant(DECIMAL_VAL(stream->ReadFloat()));
				break;
			case VAL_OBJECT:
				function->Chunk.AddConstant(
					OBJECT_VAL(TakeString(stream->ReadString())));
				break;
			}
		}

		Functions.push_back(function);
	}

	if (HasDebugInfo) {
		int tokenCount = stream->ReadInt32();
		for (int t = 0; t < tokenCount; t++) {
			char* string = stream->ReadString();
			if (!tokens) {
				stream->SkipString();
			}
			else {
				Uint32 hash = Murmur::EncryptString(string);
				if (!tokens->Exists(hash)) {
					tokens->Put(hash, string);
				}
				else {
					Memory::Free(string);
				}
			}
		}

		if (tokens) {
			for (ObjFunction* function : Functions) {
				if (tokens->Exists(function->NameHash)) {
					function->Name =
						CopyString(tokens->Get(function->NameHash));
				}
			}
		}
	}

	bool hasSourceFilename = opts & 2;
	if (hasSourceFilename) {
		SourceFilename = stream->ReadString();
	}

	stream->Close();
	return true;
}

void Bytecode::Write(Stream* stream, const char* sourceFilename, HashMap<Token>* tokenMap) {
	int hasSourceFilename = (sourceFilename != nullptr) ? 1 : 0;
	int hasDebugInfo = HasDebugInfo ? 1 : 0;

	stream->WriteBytes((char*)Bytecode::Magic, 4);
	stream->WriteByte(Version);
	stream->WriteByte((hasSourceFilename << 1) | hasDebugInfo);
	stream->WriteByte(0x00);
	stream->WriteByte(0x00);

	int chunkCount = (int)Functions.size();

	stream->WriteUInt32(chunkCount);
	for (int c = 0; c < chunkCount; c++) {
		Chunk* chunk = &Functions[c]->Chunk;

		stream->WriteUInt32(chunk->Count);
		if (Version >= 0x0001) {
			stream->WriteByte(Functions[c]->Arity);
			stream->WriteByte(Functions[c]->MinArity);
		}
		else {
			stream->WriteUInt32(Functions[c]->Arity);
		}

		if (Version >= 0x00003) {
			stream->WriteUInt32(chunk->OpcodeCount);
		}

		stream->WriteUInt32(Murmur::EncryptString(Functions[c]->Name->Chars));

		stream->WriteBytes(chunk->Code, chunk->Count);
		if (HasDebugInfo) {
			stream->WriteBytes(chunk->Lines, chunk->Count * sizeof(int));
		}

		int constSize = (int)chunk->Constants->size();
		stream->WriteUInt32(constSize);
		for (int i = 0; i < constSize; i++) {
			VMValue constt = (*chunk->Constants)[i];
			Uint8 type = (Uint8)constt.Type;
			stream->WriteByte(type);

			switch (type) {
			case VAL_INTEGER:
				stream->WriteBytes(&AS_INTEGER(constt), sizeof(int));
				break;
			case VAL_DECIMAL:
				stream->WriteBytes(&AS_DECIMAL(constt), sizeof(float));
				break;
			case VAL_OBJECT:
				if (OBJECT_TYPE(constt) == OBJ_STRING) {
					ObjString* str = AS_STRING(constt);
					stream->WriteBytes(str->Chars, str->Length + 1);
				}
				else {
					printf("Unsupported object type...Chief. (%s)\n",
						GetObjectTypeString(OBJECT_TYPE(constt)));
					stream->WriteByte(0);
				}
				break;
			}
		}
	}

	// Add tokens
	if (HasDebugInfo && tokenMap) {
		stream->WriteUInt32(tokenMap->Count + FunctionNames.size());
		std::for_each(
			FunctionNames.begin(), FunctionNames.end(), [stream](const char* name) {
				stream->WriteBytes((void*)name, strlen(name) + 1);
			});
		tokenMap->WithAll([stream](Uint32, Token t) -> void {
			stream->WriteBytes(t.Start, t.Length);
			stream->WriteByte(0); // NULL terminate
		});
	}

	if (hasSourceFilename) {
		stream->WriteBytes((void*)sourceFilename, strlen(sourceFilename) + 1);
	}
}
