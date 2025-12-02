#ifndef ENGINE_BYTECODE_TYPES_H
#define ENGINE_BYTECODE_TYPES_H

#include <Engine/Includes/HashMap.h>

#include <Engine/IO/Stream.h>

#include <Engine/Rendering/Material.h>

#include <Engine/ResourceTypes/Font.h>

#define FRAMES_MAX 64
#define STACK_SIZE_MAX (FRAMES_MAX * 256)
#define THREAD_NAME_MAX 64

#define DEFAULT_BRANCH_LIMIT 100000

typedef enum {
	ERROR_RES_EXIT,
	ERROR_RES_CONTINUE,
} ErrorResult;

typedef enum {
	VAL_NULL,
	VAL_INTEGER,
	VAL_DECIMAL,
	VAL_OBJECT,
	VAL_LINKED_INTEGER,
	VAL_LINKED_DECIMAL,
	VAL_ERROR
} ValueType;

enum { CLASS_TYPE_NORMAL, CLASS_TYPE_EXTENDED };

struct Obj;

struct VMValue {
	Uint32 Type;
	union {
		int Integer;
		float Decimal;
		Obj* Object;
		int* LinkedInteger;
		float* LinkedDecimal;
	} as;
};

#ifdef USING_VM_FUNCPTRS
class VMThread;
struct CallFrame;
typedef int (VMThread::*OpcodeFunc)(CallFrame* frame);
#endif

struct Chunk {
	int Count;
	int Capacity;
	Uint8* Code;
	Uint8* Failsafe;
	int* Lines;
	vector<VMValue>* Constants;
	bool OwnsMemory;

	int OpcodeCount;
#if USING_VM_FUNCPTRS
	OpcodeFunc* OpcodeFuncs;
	int* IPToOpcode;
#endif

	void Init();
	void Alloc();
	void Free();
#if USING_VM_FUNCPTRS
	void SetupOpfuncs();
#endif
	void Write(Uint8 byte, int line);
	int AddConstant(VMValue value);
};

struct BytecodeContainer {
	Uint8* Data;
	size_t Size;
};

const char* GetTypeString(Uint32 type);
const char* GetObjectTypeString(Uint32 type);
const char* GetValueTypeString(VMValue value);

#define IS_NULL(value) ((value).Type == VAL_NULL)
#define IS_INTEGER(value) ((value).Type == VAL_INTEGER)
#define IS_DECIMAL(value) ((value).Type == VAL_DECIMAL)
#define IS_OBJECT(value) ((value).Type == VAL_OBJECT)

#define AS_INTEGER(value) \
	(value.Type == VAL_INTEGER ? (value).as.Integer : *((value).as.LinkedInteger))
#define AS_DECIMAL(value) \
	(value.Type == VAL_DECIMAL ? (value).as.Decimal : *((value).as.LinkedDecimal))
#define AS_OBJECT(value) ((value).as.Object)

#ifdef WIN32
#define NULL_VAL (VMValue{})
static inline VMValue INTEGER_VAL(int value) {
	VMValue val;
	val.Type = VAL_INTEGER;
	val.as.Integer = value;
	return val;
}
static inline VMValue DECIMAL_VAL(float value) {
	VMValue val;
	val.Type = VAL_DECIMAL;
	val.as.Decimal = value;
	return val;
}
static inline VMValue OBJECT_VAL(void* value) {
	VMValue val;
	val.Type = VAL_OBJECT;
	val.as.Object = (Obj*)value;
	return val;
}
static inline VMValue INTEGER_LINK_VAL(int* value) {
	VMValue val;
	val.Type = VAL_LINKED_INTEGER;
	val.as.LinkedInteger = value;
	return val;
}
static inline VMValue DECIMAL_LINK_VAL(float* value) {
	VMValue val;
	val.Type = VAL_LINKED_DECIMAL;
	val.as.LinkedDecimal = value;
	return val;
}
#else
#define NULL_VAL ((VMValue){VAL_NULL, {.Integer = 0}})
#define INTEGER_VAL(value) ((VMValue){VAL_INTEGER, {.Integer = value}})
#define DECIMAL_VAL(value) ((VMValue){VAL_DECIMAL, {.Decimal = value}})
#define OBJECT_VAL(object) ((VMValue){VAL_OBJECT, {.Object = (Obj*)object}})
#define INTEGER_LINK_VAL(value) ((VMValue){VAL_LINKED_INTEGER, {.LinkedInteger = value}})
#define DECIMAL_LINK_VAL(value) ((VMValue){VAL_LINKED_DECIMAL, {.LinkedDecimal = value}})
#endif

#define IS_LINKED_INTEGER(value) ((value).Type == VAL_LINKED_INTEGER)
#define IS_LINKED_DECIMAL(value) ((value).Type == VAL_LINKED_DECIMAL)
#define AS_LINKED_INTEGER(value) (*((value).as.LinkedInteger))
#define AS_LINKED_DECIMAL(value) (*((value).as.LinkedDecimal))

#define IS_NUMBER(value) \
	(IS_DECIMAL(value) || IS_INTEGER(value) || IS_LINKED_DECIMAL(value) || \
		IS_LINKED_INTEGER(value))
#define IS_NOT_NUMBER(value) \
	(!IS_DECIMAL(value) && !IS_INTEGER(value) && !IS_LINKED_DECIMAL(value) && \
		!IS_LINKED_INTEGER(value))

typedef VMValue (*NativeFn)(int argCount, VMValue* args, Uint32 threadID);

typedef Obj* (*ClassNewFn)(void);
typedef void (*ObjectDestructor)(Obj*);

typedef bool (*ValueGetFn)(Obj* object, Uint32 hash, VMValue* value, Uint32 threadID);
typedef bool (*ValueSetFn)(Obj* object, Uint32 hash, VMValue value, Uint32 threadID);

typedef bool (*StructGetFn)(Obj* object, VMValue at, VMValue* value, Uint32 threadID);
typedef bool (*StructSetFn)(Obj* object, VMValue at, VMValue value, Uint32 threadID);

enum ObjType {
	OBJ_STRING,
	OBJ_ARRAY,
	OBJ_MAP,
	OBJ_FUNCTION,
	OBJ_BOUND_METHOD,
	OBJ_MODULE,
	OBJ_CLOSURE,
	OBJ_UPVALUE,
	OBJ_CLASS,
	OBJ_NAMESPACE,
	OBJ_ENUM,
	OBJ_INSTANCE,
	OBJ_ENTITY,
	OBJ_NATIVE_FUNCTION,
	OBJ_NATIVE_INSTANCE,

	MAX_OBJ_TYPE
};

#define CLASS_ARRAY "$$ArrayImpl"
#define CLASS_ENTITY "$$EntityImpl"
#define CLASS_FONT "Font"
#define CLASS_FUNCTION "$$FunctionImpl"
#define CLASS_INSTANCE "$$InstanceImpl"
#define CLASS_MAP "$$MapImpl"
#define CLASS_MATERIAL "Material"
#define CLASS_SHADER "Shader"
#define CLASS_STREAM "$$StreamImpl"
#define CLASS_STRING "$$StringImpl"

#define OBJECT_TYPE(value) (AS_OBJECT(value)->Type)
#define IS_BOUND_METHOD(value) IsObjectType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value) IsObjectType(value, OBJ_CLASS)
#define IS_CLOSURE(value) IsObjectType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) IsObjectType(value, OBJ_FUNCTION)
#define IS_NATIVE_FUNCTION(value) IsObjectType(value, OBJ_NATIVE_FUNCTION)
#define IS_INSTANCE(value) IsObjectType(value, OBJ_INSTANCE)
#define IS_STRING(value) IsObjectType(value, OBJ_STRING)
#define IS_ARRAY(value) IsObjectType(value, OBJ_ARRAY)
#define IS_MAP(value) IsObjectType(value, OBJ_MAP)
#define IS_NAMESPACE(value) IsObjectType(value, OBJ_NAMESPACE)
#define IS_ENUM(value) IsObjectType(value, OBJ_ENUM)
#define IS_MODULE(value) IsObjectType(value, OBJ_MODULE)
#define IS_NATIVE_INSTANCE(value) IsObjectType(value, OBJ_NATIVE_INSTANCE)
#define IS_ENTITY(value) IsObjectType(value, OBJ_ENTITY)
#define IS_INSTANCEABLE(value) (IS_INSTANCE(value) || IS_NATIVE_INSTANCE(value) || IS_ENTITY(value))
#define IS_CALLABLE(value) \
	(IS_FUNCTION(value) || IS_NATIVE_FUNCTION(value) || IS_BOUND_METHOD(value))

#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJECT(value))
#define AS_CLASS(value) ((ObjClass*)AS_OBJECT(value))
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJECT(value))
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJECT(value))
#define AS_INSTANCE(value) ((ObjInstance*)AS_OBJECT(value))
#define AS_NATIVE_FUNCTION(value) (((ObjNative*)AS_OBJECT(value))->Function)
#define AS_STRING(value) ((ObjString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJECT(value))->Chars)
#define AS_ARRAY(value) ((ObjArray*)AS_OBJECT(value))
#define AS_MAP(value) ((ObjMap*)AS_OBJECT(value))
#define AS_NAMESPACE(value) ((ObjNamespace*)AS_OBJECT(value))
#define AS_ENUM(value) ((ObjEnum*)AS_OBJECT(value))
#define AS_MODULE(value) ((ObjModule*)AS_OBJECT(value))
#define AS_ENTITY(value) ((ObjEntity*)AS_OBJECT(value))

typedef HashMap<VMValue> Table;

struct Obj {
	ObjType Type;
	size_t Size;
	bool IsDark;
	struct ObjClass* Class;
	ValueGetFn PropertyGet;
	ValueSetFn PropertySet;
	StructGetFn ElementGet;
	StructSetFn ElementSet;
	ObjectDestructor Destructor;
	struct Obj* Next;
};
struct ObjString {
	Obj Object;
	size_t Length;
	char* Chars;
	Uint32 Hash;
};
struct ObjModule {
	Obj Object;
	std::vector<struct ObjFunction*>* Functions;
	std::vector<VMValue>* Locals;
	char* SourceFilename;
};
struct ObjFunction {
	Obj Object;
	int Arity;
	int MinArity;
	int UpvalueCount;
	struct Chunk Chunk;
	ObjModule* Module;
	char* Name;
	struct ObjClass* Class;
	Uint32 NameHash;
};
struct ObjNative {
	Obj Object;
	NativeFn Function;
};
struct ObjUpvalue {
	Obj Object;
	VMValue* Value;
	VMValue Closed;
	struct ObjUpvalue* Next;
};
struct ObjClosure {
	Obj Object;
	ObjFunction* Function;
	ObjUpvalue** Upvalues;
	int UpvalueCount;
};
struct ObjClass {
	Obj Object;
	char* Name;
	Uint32 Hash;
	Table* Methods;
	Table* Fields;
	VMValue Initializer;
	ClassNewFn NewFn;
	Uint8 Type;
	ObjClass* Parent;
};
struct ObjInstance {
	Obj Object;
	Table* Fields;
};
struct ObjBoundMethod {
	Obj Object;
	VMValue Receiver;
	ObjFunction* Method;
};
struct ObjArray {
	Obj Object;
	std::vector<VMValue>* Values;
};
struct ObjMap {
	Obj Object;
	HashMap<VMValue>* Values;
	HashMap<char*>* Keys;
};
struct ObjNamespace {
	Obj Object;
	char* Name;
	Uint32 Hash;
	Table* Fields;
	bool InUse;
};
struct ObjEnum {
	Obj Object;
	char* Name;
	Uint32 Hash;
	Table* Fields;
};

#define UNION_INSTANCEABLE \
	union { \
		ObjInstance InstanceObj; \
		Obj Object; \
	}

struct ObjEntity {
	UNION_INSTANCEABLE;
	void* EntityPtr;
};
struct ObjStream {
	UNION_INSTANCEABLE;
	Stream* StreamPtr;
	bool Writable;
	bool Closed;
};
struct ObjMaterial {
	UNION_INSTANCEABLE;
	Material* MaterialPtr;
};
struct ObjShader {
	UNION_INSTANCEABLE;
	void* ShaderPtr;
};
struct ObjFont {
	UNION_INSTANCEABLE;
	Font* FontPtr;
};
struct ObjTexture {
	UNION_INSTANCEABLE;
	bool IsViewTexture;
};

#undef UNION_INSTANCEABLE

Obj* AllocateObject(size_t size, ObjType type);
ObjString* TakeString(char* chars, size_t length);
ObjString* TakeString(char* chars);
ObjString* CopyString(const char* chars, size_t length);
ObjString* CopyString(const char* chars);
ObjString* CopyString(ObjString* string);
ObjString* CopyString(std::filesystem::path path);
ObjString* AllocString(size_t length);
ObjFunction* NewFunction();
ObjNative* NewNative(NativeFn function);
ObjUpvalue* NewUpvalue(VMValue* slot);
ObjClosure* NewClosure(ObjFunction* function);
ObjClass* NewClass(Uint32 hash);
ObjClass* NewClass(const char* className);
ObjInstance* NewInstance(ObjClass* klass);
ObjEntity* NewEntity(ObjClass* klass);
ObjBoundMethod* NewBoundMethod(VMValue receiver, ObjFunction* method);
ObjArray* NewArray();
ObjMap* NewMap();
ObjNamespace* NewNamespace(Uint32 hash);
ObjNamespace* NewNamespace(const char* nsName);
ObjEnum* NewEnum(Uint32 hash);
ObjModule* NewModule();
Obj* NewNativeInstance(size_t size);

#define FREE_OBJ(obj) \
	assert(GarbageCollector::GarbageSize >= ((Obj*)(obj))->Size); \
	GarbageCollector::GarbageSize -= ((Obj*)(obj))->Size; \
	Memory::Free(obj)

std::string GetClassName(Uint32 hash);
Uint32 GetClassHash(const char* name);

static inline bool IsObjectType(VMValue value, ObjType type) {
	return IS_OBJECT(value) && AS_OBJECT(value)->Type == type;
}
static inline bool IsNativeInstance(Obj* object, const char* className) {
	if (object->Type != OBJ_NATIVE_INSTANCE) {
		return false;
	}

	ObjClass* klass = object->Class;
	if (klass != nullptr && klass->Hash == GetClassHash(className)) {
		return true;
	}

	return false;
}
static inline bool IsNativeInstance(VMValue value, const char* className) {
	if (!IS_OBJECT(value)) {
		return false;
	}

	return IsNativeInstance(AS_OBJECT(value), className);
}
static inline bool HasInitializer(ObjClass* klass) {
	return !IS_NULL(klass->Initializer);
}

struct WithIter {
	void* entity;
	void* entityNext;
	int index;
	void* registry;
	Uint8 receiverSlot;
};

struct CallFrame {
	ObjFunction* Function;
	Uint8* IP;
	Uint8* IPLast;
	Uint8* IPStart;
	VMValue* Slots;
	ObjModule* Module;

#ifdef VM_DEBUG
	Uint32 BranchCount;
#endif

#ifdef USING_VM_FUNCPTRS
	OpcodeFunc* OpcodeFStart;
	int* IPToOpcode;
	OpcodeFunc* OpcodeFunctions;
#endif

	VMValue WithReceiverStack[16];
	VMValue* WithReceiverStackTop = WithReceiverStack;
	WithIter WithIteratorStack[16];
	WithIter* WithIteratorStackTop = WithIteratorStack;
};
enum OpCode : uint8_t {
	OP_ERROR = 0,
	OP_CONSTANT,
	// Classes and Instances
	OP_DEFINE_GLOBAL,
	OP_GET_PROPERTY,
	OP_SET_PROPERTY,
	OP_GET_GLOBAL,
	OP_SET_GLOBAL,
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	//
	OP_PRINT_STACK,
	//
	OP_INHERIT,
	OP_RETURN,
	OP_METHOD_V4,
	OP_CLASS,
	// Function Operations
	OP_CALL,
	OP_SUPER,
	OP_INVOKE_V3,
	// Jumping
	OP_JUMP,
	OP_JUMP_IF_FALSE,
	OP_JUMP_BACK,
	// Stack Operation
	OP_POP,
	OP_COPY,
	// Numeric Operations
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_MODULO,
	OP_NEGATE,
	OP_INCREMENT,
	OP_DECREMENT,
	// Bit Operations
	OP_BITSHIFT_LEFT,
	OP_BITSHIFT_RIGHT,
	// Constants
	OP_NULL,
	OP_TRUE,
	OP_FALSE,
	// Bitwise Operations
	OP_BW_NOT,
	OP_BW_AND,
	OP_BW_OR,
	OP_BW_XOR,
	// Logical Operations
	OP_LG_NOT,
	OP_LG_AND,
	OP_LG_OR,
	// Equality and Comparison Operators
	OP_EQUAL,
	OP_EQUAL_NOT,
	OP_GREATER,
	OP_GREATER_EQUAL,
	OP_LESS,
	OP_LESS_EQUAL,
	//
	OP_PRINT,
	OP_ENUM_NEXT,
	OP_SAVE_VALUE,
	OP_LOAD_VALUE,
	OP_WITH,
	OP_GET_ELEMENT,
	OP_SET_ELEMENT,
	OP_NEW_ARRAY,
	OP_NEW_MAP,
	//
	OP_SWITCH_TABLE,
	OP_FAILSAFE,
	OP_EVENT_V4,
	OP_TYPEOF,
	OP_NEW,
	OP_IMPORT,
	OP_SWITCH,
	OP_POPN,
	OP_HAS_PROPERTY,
	OP_IMPORT_MODULE,
	OP_ADD_ENUM,
	OP_NEW_ENUM,
	OP_GET_SUPERCLASS,
	OP_GET_MODULE_LOCAL,
	OP_SET_MODULE_LOCAL,
	OP_DEFINE_MODULE_LOCAL,
	OP_USE_NAMESPACE,
	OP_DEFINE_CONSTANT,
	OP_INTEGER,
	OP_DECIMAL,
	OP_INVOKE,
	OP_SUPER_INVOKE,
	OP_EVENT,
	OP_METHOD,

	OP_LAST
};

#endif /* ENGINE_BYTECODE_TYPES_H */
