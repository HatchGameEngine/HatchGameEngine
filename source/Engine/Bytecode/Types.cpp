#include <Engine/Bytecode/Types.h>

#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/TypeImpl/ArrayImpl.h>
#include <Engine/Bytecode/TypeImpl/EntityImpl.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/MapImpl.h>
#include <Engine/Bytecode/TypeImpl/MaterialImpl.h>
#include <Engine/Bytecode/TypeImpl/ShaderImpl.h>
#include <Engine/Bytecode/TypeImpl/StreamImpl.h>
#include <Engine/Bytecode/TypeImpl/StringImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Hashing/FNV1A.h>

#define ALLOCATE_OBJ(type, objectType) (type*)AllocateObject(sizeof(type), objectType)
#define ALLOCATE(type, size) (type*)Memory::TrackedMalloc(#type, sizeof(type) * size)

#define GROW_CAPACITY(val) ((val) < 8 ? 8 : val << 1)

Obj* AllocateObject(size_t size, ObjType type) {
	// Only do this when allocating more memory
	GarbageCollector::GarbageSize += size;

	Obj* object = (Obj*)Memory::TrackedCalloc("AllocateObject", 1, size);
	object->Size = size;
	object->Type = type;
	object->Next = GarbageCollector::RootObject;
	GarbageCollector::RootObject = object;

	return object;
}

static ObjString* AllocateString(char* chars, size_t length, Uint32 hash) {
	return (ObjString*)StringImpl::New(chars, length, hash);
}

ObjString* TakeString(char* chars, size_t length) {
	Uint32 hash = FNV1A::EncryptData(chars, length);
	return AllocateString(chars, length, hash);
}
ObjString* TakeString(char* chars) {
	return TakeString(chars, strlen(chars));
}
ObjString* CopyString(const char* chars, size_t length) {
	Uint32 hash = FNV1A::EncryptData(chars, length);

	char* heapChars = ALLOCATE(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';

	return AllocateString(heapChars, length, hash);
}
ObjString* CopyString(const char* chars) {
	return CopyString(chars, strlen(chars));
}
ObjString* CopyString(ObjString* string) {
	char* heapChars = ALLOCATE(char, string->Length + 1);
	memcpy(heapChars, string->Chars, string->Length);
	heapChars[string->Length] = '\0';

	return AllocateString(heapChars, string->Length, string->Hash);
}
ObjString* CopyString(std::filesystem::path path) {
	std::string asStr = Path::ToString(path);
	const char* cStr = asStr.c_str();
	return CopyString(cStr);
}
ObjString* AllocString(size_t length) {
	char* heapChars = ALLOCATE(char, length + 1);
	heapChars[length] = '\0';

	return AllocateString(heapChars, length, 0x00000000);
}

ObjFunction* NewFunction() {
	return (ObjFunction*)FunctionImpl::New();
}
ObjNative* NewNative(NativeFn function) {
	ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE_FUNCTION);
	Memory::Track(native, "NewNative");
	native->Function = function;
	return native;
}
ObjUpvalue* NewUpvalue(VMValue* slot) {
	ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
	upvalue->Closed = NULL_VAL;
	upvalue->Value = slot;
	return upvalue;
}
ObjClosure* NewClosure(ObjFunction* function) {
	ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->UpvalueCount);
	for (int i = 0; i < function->UpvalueCount; i++) {
		upvalues[i] = NULL;
	}

	ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
	closure->Function = function;
	closure->Upvalues = upvalues;
	closure->UpvalueCount = function->UpvalueCount;
	return closure;
}
ObjClass* NewClass(Uint32 hash) {
	ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
	Memory::Track(klass, "NewClass");
	klass->Hash = hash;
	klass->Object.Destructor = ScriptManager::FreeClass;
	klass->Methods = new Table();
	klass->Fields = new Table();
	klass->Initializer = NULL_VAL;
	klass->Type = CLASS_TYPE_NORMAL;
	klass->Name = StringUtils::Create(GetClassName(hash));
	return klass;
}
ObjClass* NewClass(const char* className) {
	ObjClass* klass = NewClass(GetClassHash(className));
	klass->Name = (char*)Memory::Realloc(klass->Name, strlen(className) + 1);
	memcpy(klass->Name, className, strlen(className) + 1);
	return klass;
}
ObjInstance* NewInstance(ObjClass* klass) {
	ObjInstance* instance = (ObjInstance*)InstanceImpl::New(sizeof(ObjInstance), OBJ_INSTANCE);
	instance->Object.Class = klass;
	return instance;
}
ObjEntity* NewEntity(ObjClass* klass) {
	return (ObjEntity*)EntityImpl::New(klass);
}
ObjBoundMethod* NewBoundMethod(VMValue receiver, ObjFunction* method) {
	ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
	Memory::Track(bound, "NewBoundMethod");
	bound->Receiver = receiver;
	bound->Method = method;
	return bound;
}
ObjArray* NewArray() {
	return (ObjArray*)ArrayImpl::New();
}
ObjMap* NewMap() {
	return (ObjMap*)MapImpl::New();
}
ObjNamespace* NewNamespace(Uint32 hash) {
	ObjNamespace* ns = ALLOCATE_OBJ(ObjNamespace, OBJ_NAMESPACE);
	Memory::Track(ns, "NewNamespace");
	ns->Object.Destructor = ScriptManager::FreeNamespace;
	ns->Hash = hash;
	ns->Fields = new Table();
	ns->Name = StringUtils::Create(GetClassName(hash));
	return ns;
}
ObjNamespace* NewNamespace(const char* nsName) {
	ObjNamespace* ns = NewNamespace(GetClassHash(nsName));
	ns->Name = (char*)Memory::Realloc(ns->Name, strlen(nsName) + 1);
	memcpy(ns->Name, nsName, strlen(nsName) + 1);
	return ns;
}
ObjEnum* NewEnum(Uint32 hash) {
	ObjEnum* enumeration = ALLOCATE_OBJ(ObjEnum, OBJ_ENUM);
	Memory::Track(enumeration, "NewEnum");
	enumeration->Object.Destructor = ScriptManager::FreeEnumeration;
	enumeration->Hash = hash;
	enumeration->Fields = new Table();
	enumeration->Name = StringUtils::Create(GetClassName(hash));
	return enumeration;
}
ObjModule* NewModule() {
	ObjModule* module = ALLOCATE_OBJ(ObjModule, OBJ_MODULE);
	Memory::Track(module, "NewModule");
	module->Object.Destructor = ScriptManager::FreeModule;
	module->Functions = new vector<ObjFunction*>();
	module->Locals = new vector<VMValue>();
	return module;
}
Obj* NewNativeInstance(size_t size) {
	Obj* obj = InstanceImpl::New(size, OBJ_NATIVE_INSTANCE);
	Memory::Track(obj, "NewNativeInstance");
	return obj;
}

std::string GetClassName(Uint32 hash) {
	if (ScriptManager::Tokens && ScriptManager::Tokens->Exists(hash)) {
		char* t = ScriptManager::Tokens->Get(hash);
		return std::string(t);
	}
	else {
		char nameHash[9];
		snprintf(nameHash, sizeof(nameHash), "%8X", hash);
		return std::string(nameHash);
	}
}
Uint32 GetClassHash(const char* name) {
	return Murmur::EncryptString(name);
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
#if USING_VM_FUNCPTRS
	OpcodeFuncs = NULL;
	IPToOpcode = NULL;
#endif
	Constants = new vector<VMValue>();
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
#if USING_VM_FUNCPTRS
#define OPCASE(op) \
	case op: \
		func = &VMThread::OPFUNC_##op; \
		break
void Chunk::SetupOpfuncs() {
	if (!OpcodeCount) {
		// try to get it manually thru iterating it (for
		// version <= 2 bytecode)
		for (int offset = 0; offset < Count;) {
			offset += Compiler::GetTotalOpcodeSize(Code + offset);
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
		offset += Compiler::GetTotalOpcodeSize(Code + offset);
		OpcodeFunc func = NULL;
		switch (op) {
			OPCASE(OP_ERROR);
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
			OPCASE(OP_SUPER);
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
			OPCASE(OP_FAILSAFE);
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
