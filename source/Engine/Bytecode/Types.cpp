#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Diagnostics/Memory.h>

#define GROW_CAPACITY(val) ((val) < 8 ? 8 : val << 1)

Uint32 GetClassHash(const char* name) {
	return Murmur::EncryptString(name);
}

const char* GetModuleName(ObjModule* module) {
	if (module->SourceFilename) {
		return module->SourceFilename;
	}

	return "repl";
}

const char* GetTypeString(Uint32 type) {
	switch (type) {
	case VAL_NULL:
		return "null";
	case VAL_INTEGER:
	case VAL_LINKED_INTEGER:
		return "integer";
	case VAL_DECIMAL:
	case VAL_LINKED_DECIMAL:
		return "decimal";
	case VAL_HITBOX:
		return "hitbox";
	case VAL_OBJECT:
		return "object";
	case VAL_LOCATION:
		return "location";
	}
	return "unknown type";
}
const char* GetObjectTypeString(Uint32 type) {
	return Value::GetObjectTypeName(type);
}
const char* GetValueTypeString(VMValue value) {
	if (value.Type == VAL_OBJECT) {
		return GetObjectTypeString(OBJECT_TYPE(value));
	}
	else {
		return GetTypeString(value.Type);
	}
}

void Chunk::Init() {
	Count = 0;
	Capacity = 0;
	Code = NULL;
	Lines = NULL;
	Breakpoints = NULL;
	BreakpointCount = 0;
#if USING_VM_FUNCPTRS
	OpcodeFuncs = NULL;
	IPToOpcode = NULL;
#endif
	Constants = new vector<VMValue>();
	Locals = nullptr;
	ModuleLocals = nullptr;
}
void Chunk::Alloc() {
	if (!Code) {
		Code = (Uint8*)Memory::TrackedMalloc("Chunk::Code", sizeof(Uint8) * Capacity);
	}
	else {
		Code = (Uint8*)Memory::Realloc(Code, sizeof(Uint8) * Capacity);
	}

	if (!Lines) {
		Lines = (int*)Memory::TrackedMalloc("Chunk::Lines", sizeof(int) * Capacity);
	}
	else {
		Lines = (int*)Memory::Realloc(Lines, sizeof(int) * Capacity);
	}

	OwnsMemory = true;
}
void Chunk::Free() {
	if (OwnsMemory) {
		if (Code) {
			Memory::Free(Code);
			Code = NULL;
			Count = 0;
			Capacity = 0;
		}
		if (Lines) {
			Memory::Free(Lines);
			Lines = NULL;
		}
	}

	if (Constants) {
		Constants->clear();
		Constants->shrink_to_fit();
		delete Constants;
	}

	if (Breakpoints) {
		Memory::Free(Breakpoints);
	}

	if (Locals) {
		DeleteLocals(Locals);
	}
	if (ModuleLocals) {
		DeleteLocals(ModuleLocals);
	}

#if USING_VM_FUNCPTRS
	if (OpcodeFuncs) {
		Memory::Free(OpcodeFuncs);
		Memory::Free(IPToOpcode);
		OpcodeFuncs = NULL;
		IPToOpcode = NULL;
		OpcodeCount = 0;
	}
#endif
}
void Chunk::DeleteLocals(vector<ChunkLocal>* locals) {
	for (size_t i = 0; i < locals->size(); i++) {
		Memory::Free((*locals)[i].Name);
	}
	locals->clear();
	locals->shrink_to_fit();
	delete locals;
}
#if USING_VM_FUNCPTRS
#define OPCASE(op) \
	case op: \
		func = &VMThread::OPFUNC_##op; \
		break
void Chunk::SetupOpfuncs() {
	if (!OpcodeCount) {
		// try to get it manually thru iterating it (for version <= 2 bytecode)
		for (int offset = 0; offset < Count;) {
			offset += Bytecode::GetTotalOpcodeSize(Code + offset);
			OpcodeCount++;
		}
	}

	OpcodeFuncs = (OpcodeFunc*)Memory::TrackedMalloc(
		"Chunk::OpcodeFuncs", sizeof(OpcodeFunc) * OpcodeCount);
	IPToOpcode = (int*)Memory::TrackedMalloc("Chunk::IPToOpcode", sizeof(int) * Count);
	int offset = 0;
	for (int i = 0; i < OpcodeCount; i++) {
		Uint8 op = *(Code + offset);
		IPToOpcode[offset] = i;
		offset += Bytecode::GetTotalOpcodeSize(Code + offset);
		OpcodeFunc func = NULL;
		switch (op) {
			OPCASE(OP_NOP);
			OPCASE(OP_CONSTANT);
			OPCASE(OP_DEFINE_GLOBAL);
			OPCASE(OP_GET_PROPERTY);
			OPCASE(OP_SET_PROPERTY);
			OPCASE(OP_GET_GLOBAL);
			OPCASE(OP_SET_GLOBAL);
			OPCASE(OP_GET_LOCAL);
			OPCASE(OP_SET_LOCAL);
			OPCASE(OP_PRINT_STACK);
			OPCASE(OP_INHERIT);
			OPCASE(OP_RETURN);
			OPCASE(OP_METHOD_V4);
			OPCASE(OP_CLASS);
			OPCASE(OP_CALL);
			OPCASE(OP_UNUSED_1);
			OPCASE(OP_INVOKE_V3);
			OPCASE(OP_JUMP);
			OPCASE(OP_JUMP_IF_FALSE);
			OPCASE(OP_JUMP_BACK);
			OPCASE(OP_POP);
			OPCASE(OP_COPY);
			OPCASE(OP_ADD);
			OPCASE(OP_SUBTRACT);
			OPCASE(OP_MULTIPLY);
			OPCASE(OP_DIVIDE);
			OPCASE(OP_MODULO);
			OPCASE(OP_NEGATE);
			OPCASE(OP_INCREMENT);
			OPCASE(OP_DECREMENT);
			OPCASE(OP_BITSHIFT_LEFT);
			OPCASE(OP_BITSHIFT_RIGHT);
			OPCASE(OP_NULL);
			OPCASE(OP_TRUE);
			OPCASE(OP_FALSE);
			OPCASE(OP_BW_NOT);
			OPCASE(OP_BW_AND);
			OPCASE(OP_BW_OR);
			OPCASE(OP_BW_XOR);
			OPCASE(OP_LG_NOT);
			OPCASE(OP_LG_AND);
			OPCASE(OP_LG_OR);
			OPCASE(OP_EQUAL);
			OPCASE(OP_EQUAL_NOT);
			OPCASE(OP_GREATER);
			OPCASE(OP_GREATER_EQUAL);
			OPCASE(OP_LESS);
			OPCASE(OP_LESS_EQUAL);
			OPCASE(OP_PRINT);
			OPCASE(OP_ENUM_NEXT);
			OPCASE(OP_SAVE_VALUE);
			OPCASE(OP_LOAD_VALUE);
			OPCASE(OP_WITH);
			OPCASE(OP_GET_ELEMENT);
			OPCASE(OP_SET_ELEMENT);
			OPCASE(OP_NEW_ARRAY);
			OPCASE(OP_NEW_MAP);
			OPCASE(OP_SWITCH_TABLE);
			OPCASE(OP_UNUSED_2);
			OPCASE(OP_EVENT_V4);
			OPCASE(OP_TYPEOF);
			OPCASE(OP_NEW);
			OPCASE(OP_IMPORT);
			OPCASE(OP_SWITCH);
			OPCASE(OP_POPN);
			OPCASE(OP_HAS_PROPERTY);
			OPCASE(OP_IMPORT_MODULE);
			OPCASE(OP_ADD_ENUM);
			OPCASE(OP_NEW_ENUM);
			OPCASE(OP_GET_SUPERCLASS);
			OPCASE(OP_GET_MODULE_LOCAL);
			OPCASE(OP_SET_MODULE_LOCAL);
			OPCASE(OP_DEFINE_MODULE_LOCAL);
			OPCASE(OP_USE_NAMESPACE);
			OPCASE(OP_DEFINE_CONSTANT);
			OPCASE(OP_INTEGER);
			OPCASE(OP_DECIMAL);
			OPCASE(OP_INVOKE);
			OPCASE(OP_SUPER_INVOKE);
			OPCASE(OP_EVENT);
			OPCASE(OP_METHOD);
			OPCASE(OP_NEW_HITBOX);
			OPCASE(OP_LOCATION_STACK);
			OPCASE(OP_LOCATION_MODULE_LOCAL);
			OPCASE(OP_LOCATION_GLOBAL);
			OPCASE(OP_LOCATION_PROPERTY);
			OPCASE(OP_LOCATION_SUPER_PROPERTY);
			OPCASE(OP_LOCATION_ELEMENT);
			OPCASE(OP_LOAD_INDIRECT);
			OPCASE(OP_STORE_INDIRECT);
		}
		assert((func != NULL));
		OpcodeFuncs[i] = func;
	}
}
#undef OPCASE
#endif
void Chunk::Write(Uint8 byte, int line) {
	if (Capacity < Count + 1) {
		int oldCapacity = Capacity;
		Capacity = GROW_CAPACITY(oldCapacity);
		Alloc();
	}
	Code[Count] = byte;
	Lines[Count] = line;
	Count++;
}
int Chunk::AddConstant(VMValue value) {
	Constants->push_back(value);
	return (int)Constants->size() - 1;
}
bool Chunk::GetConstant(size_t offset, VMValue* value, int* index) {
	Uint8* code = Code + offset;
	if (index) {
		*index = -1;
	}
	switch (*code) {
	case OP_CONSTANT:
		if (value) {
			*value = (*Constants)[*(Uint32*)(code + 1)];
		}
		if (index) {
			*index = *(Uint32*)(code + 1);
		}
		return true;
	case OP_FALSE:
	case OP_TRUE:
		if (value) {
			*value = INTEGER_VAL(*code == OP_FALSE ? 0 : 1);
		}
		return true;
	case OP_NULL:
		if (value) {
			*value = NULL_VAL;
		}
		return true;
	case OP_INTEGER:
		if (value) {
			*value = INTEGER_VAL(*(Sint32*)(code + 1));
		}
		return true;
	case OP_DECIMAL:
		if (value) {
			*value = DECIMAL_VAL(*(float*)(code + 1));
		}
		return true;
	}

	return false;
}
