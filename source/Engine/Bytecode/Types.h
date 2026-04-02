#ifndef ENGINE_BYTECODE_TYPES_H
#define ENGINE_BYTECODE_TYPES_H

#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/OrderedHashMap.h>

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
	VAL_HITBOX,
	VAL_COLOR,
	VAL_LOCATION,
	VAL_ERROR
} ValueType;

enum { CLASS_TYPE_NORMAL, CLASS_TYPE_EXTENDED };

struct Obj;

enum {
	LOC_STACK,
	LOC_MODULE,
	LOC_GLOBAL,
	LOC_PROPERTY,
	LOC_ELEMENT
};

struct VMColor {
	float Red;
	float Green;
	float Blue;
	float Alpha;

	VMColor operator+(const VMColor& b) {
		VMColor result;
		result.Red = this->Red + b.Red;
		result.Green = this->Green + b.Green;
		result.Blue = this->Blue + b.Blue;
		result.Alpha = this->Alpha + b.Alpha;
		return result;
	}

	VMColor operator-(const VMColor& b) {
		VMColor result;
		result.Red = this->Red - b.Red;
		result.Green = this->Green - b.Green;
		result.Blue = this->Blue - b.Blue;
		result.Alpha = this->Alpha - b.Alpha;
		return result;
	}

	VMColor operator*(const VMColor& b) {
		VMColor result;
		result.Red = this->Red * b.Red;
		result.Green = this->Green * b.Green;
		result.Blue = this->Blue * b.Blue;
		result.Alpha = this->Alpha * b.Alpha;
		return result;
	}

	VMColor operator*(const float& b) {
		VMColor result;
		result.Red = this->Red * b;
		result.Green = this->Green * b;
		result.Blue = this->Blue * b;
		result.Alpha = this->Alpha * b;
		return result;
	}

	VMColor operator*(const int& b) {
		VMColor result;
		result.Red = this->Red * b;
		result.Green = this->Green * b;
		result.Blue = this->Blue * b;
		result.Alpha = this->Alpha * b;
		return result;
	}

	VMColor operator/(const VMColor& b) {
		VMColor result;
		result.Red = this->Red / b.Red;
		result.Green = this->Green / b.Green;
		result.Blue = this->Blue / b.Blue;
		result.Alpha = this->Alpha / b.Alpha;
		return result;
	}

	VMColor operator/(const float& b) {
		VMColor result;
		result.Red = this->Red / b;
		result.Green = this->Green / b;
		result.Blue = this->Blue / b;
		result.Alpha = this->Alpha / b;
		return result;
	}

	VMColor operator/(const int& b) {
		VMColor result;
		result.Red = this->Red / b;
		result.Green = this->Green / b;
		result.Blue = this->Blue / b;
		result.Alpha = this->Alpha / b;
		return result;
	}

	int AsRGB() {
		int red = std::max(0, std::min((int)(Red * 255.0f), 0xFF));
		int green = std::max(0, std::min((int)(Green * 255.0f), 0xFF));
		int blue = std::max(0, std::min((int)(Blue * 255.0f), 0xFF));

#if HATCH_BIG_ENDIAN
		return red << 24 | green << 16 | blue << 8 | 0xFF;
#else
		return 0xFF000000 | red << 16 | green << 8 | blue;
#endif
	}

	int AsRGBA() {
		int red = std::max(0, std::min((int)(Red * 255.0f), 0xFF));
		int green = std::max(0, std::min((int)(Green * 255.0f), 0xFF));
		int blue = std::max(0, std::min((int)(Blue * 255.0f), 0xFF));
		int alpha = std::max(0, std::min((int)(Alpha * 255.0f), 0xFF));

#if HATCH_BIG_ENDIAN
		return red << 24 | green << 16 | blue << 8 | alpha;
#else
		return alpha << 24 | red << 16 | green << 8 | blue;
#endif
	}
};

typedef union {
	int Integer;
	float Decimal;
	Obj* Object;
	int* LinkedInteger;
	float* LinkedDecimal;
	Sint16 Hitbox[NUM_HITBOX_SIDES];
	VMColor Color;
} VMValueUnion;

struct VMLocation {
	Uint8 Type;
	union {
		struct VMValue* Slot;
		Uint32 Global;
		struct {
			VMValueUnion Object;
			Uint8 ObjectType;
			Uint32 Hash;
		} Property;
		struct {
			VMValueUnion Object;
			Uint8 ObjectType;
			VMValueUnion At;
			Uint8 AtType;
		} Element;
	} as;
};

struct VMValue {
	Uint8 Type;
	union {
		VMValueUnion Value;
		VMLocation Location;
	} as;
};

#ifdef USING_VM_FUNCPTRS
class VMThread;
struct CallFrame;
typedef int (VMThread::*OpcodeFunc)(CallFrame* frame);
#endif

#define CHUNKLOCAL_FLAG_RESOLVED (1 << 0)
#define CHUNKLOCAL_FLAG_CONST (1 << 1)

#define MAX_CHUNK_BREAKPOINTS 0xFFFF

struct ChunkLocal {
	char* Name;
	bool Constant;
	bool Resolved;
	Uint32 Index;
	Uint32 Position;
};

struct Chunk {
	int Count;
	int Capacity;
	Uint8* Code;
	Uint32* Breakpoints;
	Uint16 BreakpointCount;
	int* Lines;
	vector<VMValue>* Constants;
	vector<ChunkLocal>* Locals;
	vector<ChunkLocal>* ModuleLocals;
	bool OwnsMemory;

	int OpcodeCount;
#if USING_VM_FUNCPTRS
	OpcodeFunc* OpcodeFuncs;
	int* IPToOpcode;
#endif

	void Init();
	void Alloc();
	void Free();
	void DeleteLocals(vector<ChunkLocal>* locals);
#if USING_VM_FUNCPTRS
	void SetupOpfuncs();
#endif
	void Write(Uint8 byte, int line);
	int AddConstant(VMValue value);
	bool GetConstant(size_t offset, VMValue* value = NULL, int* index = NULL);
};

struct BytecodeContainer {
	Uint8* Data;
	size_t Size;
};

#ifdef VM_DEBUG
struct SourceFile {
	bool Exists = false;
	char* Text = nullptr;
	std::vector<char*> Lines;
};
#endif

const char* GetTypeString(Uint32 type);
const char* GetObjectTypeString(Uint32 type);
const char* GetValueTypeString(VMValue value);

#define IS_NULL(value) ((value).Type == VAL_NULL)
#define IS_INTEGER(value) ((value).Type == VAL_INTEGER)
#define IS_DECIMAL(value) ((value).Type == VAL_DECIMAL)
#define IS_OBJECT(value) ((value).Type == VAL_OBJECT)
#define IS_LOCATION(value) ((value).Type == VAL_LOCATION)

#define AS_INTEGER(value) \
	(value.Type == VAL_INTEGER ? (value).as.Value.Integer : *((value).as.Value.LinkedInteger))
#define AS_DECIMAL(value) \
	(value.Type == VAL_DECIMAL ? (value).as.Value.Decimal : *((value).as.Value.LinkedDecimal))
#define AS_OBJECT(value) ((value).as.Value.Object)
#define AS_LOCATION(value) ((value).as.Location)

#ifdef WIN32
#define NULL_VAL (VMValue{})
static inline VMValue INTEGER_VAL(int value) {
	VMValue val;
	val.Type = VAL_INTEGER;
	val.as.Value.Integer = value;
	return val;
}
static inline VMValue DECIMAL_VAL(float value) {
	VMValue val;
	val.Type = VAL_DECIMAL;
	val.as.Value.Decimal = value;
	return val;
}
static inline VMValue OBJECT_VAL(void* value) {
	VMValue val;
	val.Type = VAL_OBJECT;
	val.as.Value.Object = (Obj*)value;
	return val;
}
static inline VMValue LOCATION_VAL(VMLocation location) {
	VMValue val;
	val.Type = VAL_LOCATION;
	val.as.Location = location;
	return val;
}
static inline VMValue INTEGER_LINK_VAL(int* value) {
	VMValue val;
	val.Type = VAL_LINKED_INTEGER;
	val.as.Value.LinkedInteger = value;
	return val;
}
static inline VMValue DECIMAL_LINK_VAL(float* value) {
	VMValue val;
	val.Type = VAL_LINKED_DECIMAL;
	val.as.Value.LinkedDecimal = value;
	return val;
}
#else
#define NULL_VAL ((VMValue){VAL_NULL, {{.Integer = 0}}})
#define INTEGER_VAL(value) ((VMValue){VAL_INTEGER, {{.Integer = value}}})
#define DECIMAL_VAL(value) ((VMValue){VAL_DECIMAL, {{.Decimal = value}}})
#define OBJECT_VAL(object) ((VMValue){VAL_OBJECT, {{.Object = (Obj*)object}}})
#define LOCATION_VAL(location) ((VMValue){VAL_LOCATION, {.Location = location}})
#define INTEGER_LINK_VAL(value) ((VMValue){VAL_LINKED_INTEGER, {{.LinkedInteger = value}}})
#define DECIMAL_LINK_VAL(value) ((VMValue){VAL_LINKED_DECIMAL, {{.LinkedDecimal = value}}})
#endif

#define IS_LINKED_INTEGER(value) ((value).Type == VAL_LINKED_INTEGER)
#define IS_LINKED_DECIMAL(value) ((value).Type == VAL_LINKED_DECIMAL)
#define AS_LINKED_INTEGER(value) (*((value).as.Value.LinkedInteger))
#define AS_LINKED_DECIMAL(value) (*((value).as.Value.LinkedDecimal))

#define IS_NUMBER(value) \
	(IS_DECIMAL(value) || IS_INTEGER(value) || IS_LINKED_DECIMAL(value) || \
		IS_LINKED_INTEGER(value))
#define IS_NOT_NUMBER(value) \
	(!IS_DECIMAL(value) && !IS_INTEGER(value) && !IS_LINKED_DECIMAL(value) && \
		!IS_LINKED_INTEGER(value))

#define IS_HITBOX(value) ((value).Type == VAL_HITBOX)
#define AS_HITBOX(value) (&((value).as.Value.Hitbox[0]))

static inline VMValue HITBOX_VAL(Sint16 left, Sint16 top, Sint16 right, Sint16 bottom) {
	VMValue val;
	val.Type = VAL_HITBOX;
	val.as.Value.Hitbox[HITBOX_LEFT] = left;
	val.as.Value.Hitbox[HITBOX_TOP] = top;
	val.as.Value.Hitbox[HITBOX_RIGHT] = right;
	val.as.Value.Hitbox[HITBOX_BOTTOM] = bottom;
	return val;
}

static inline VMValue HITBOX_VAL(Sint16* values) {
	VMValue val;
	val.Type = VAL_HITBOX;
	val.as.Value.Hitbox[HITBOX_LEFT] = values[HITBOX_LEFT];
	val.as.Value.Hitbox[HITBOX_TOP] = values[HITBOX_TOP];
	val.as.Value.Hitbox[HITBOX_RIGHT] = values[HITBOX_RIGHT];
	val.as.Value.Hitbox[HITBOX_BOTTOM] = values[HITBOX_BOTTOM];
	return val;
}

#define IS_COLOR(value) ((value).Type == VAL_COLOR)
#define AS_COLOR(value) ((value).as.Value.Color)

static inline VMValue COLOR_VAL(float red, float green, float blue, float alpha) {
	VMValue val;
	val.Type = VAL_COLOR;
	AS_COLOR(val).Red = red;
	AS_COLOR(val).Green = green;
	AS_COLOR(val).Blue = blue;
	AS_COLOR(val).Alpha = alpha;
	return val;
}

static inline VMValue COLOR_VAL(VMColor color) {
	VMValue val;
	val.Type = VAL_COLOR;
	AS_COLOR(val) = color;
	return val;
}

#ifdef WIN32
static inline VMLocation STACK_LOCATION(VMValue* slot) {
	VMLocation location;
	location.Type = LOC_STACK;
	location.as.Slot = slot;
	return location;
}
static inline VMLocation MODULE_LOCATION(VMValue* slot) {
	VMLocation location;
	location.Type = LOC_MODULE;
	location.as.Slot = slot;
	return location;
}
static inline VMLocation GLOBAL_LOCATION(Uint32 hash) {
	VMLocation location;
	location.Type = LOC_GLOBAL;
	location.as.Global = hash;
	return location;
}
#else
#define STACK_LOCATION(slot) ((VMLocation){LOC_STACK, {.Slot = slot}})
#define MODULE_LOCATION(slot) ((VMLocation){LOC_MODULE, {.Slot = slot}})
#define GLOBAL_LOCATION(hash) ((VMLocation){LOC_GLOBAL, {.Global = hash}})
#endif

static inline VMLocation PROPERTY_LOCATION(VMValue object, Uint32 hash) {
	VMLocation location;
	location.Type = LOC_PROPERTY;
	location.as.Property.ObjectType = object.Type;
	location.as.Property.Object = object.as.Value;
	location.as.Property.Hash = hash;
	return location;
}
static inline VMLocation ELEMENT_LOCATION(VMValue object, VMValue at) {
	VMLocation location;
	location.Type = LOC_ELEMENT;
	location.as.Element.ObjectType = object.Type;
	location.as.Element.Object = object.as.Value;
	location.as.Element.AtType = at.Type;
	location.as.Element.At = at.as.Value;
	return location;
}

static inline bool IsStructureLocation(VMLocation location) {
	return location.Type == LOC_PROPERTY || location.Type == LOC_ELEMENT;
}

static inline bool IsStructureLocationValueObject(VMLocation location) {
	if (IsStructureLocation(location)) {
		switch (location.Type) {
		case LOC_PROPERTY:
			return location.as.Property.ObjectType == VAL_OBJECT;
		case LOC_ELEMENT:
			return location.as.Element.ObjectType == VAL_OBJECT;
		}
	}

	return false;
}

typedef VMValue (*NativeFn)(int argCount, VMValue* args, Uint32 threadID);

typedef VMValue (*ClassNewFn)(void);
typedef void (*ObjectDestructor)(Obj*);

typedef bool (*ValueGetFn)(VMValue instance, Uint32 hash, VMValue* value, Uint32 threadID);
typedef bool (*ValueSetFn)(VMValue& instance, Uint32 hash, VMValue value, Uint32 threadID);

typedef bool (*ElementGetFn)(VMValue instance, VMValue at, VMValue* value, Uint32 threadID);
typedef bool (*ElementSetFn)(VMValue& instance, VMValue at, VMValue value, Uint32 threadID);

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

#define CLASS_ARRAY "ArrayImpl"
#define CLASS_ENTITY "EntityImpl"
#define CLASS_COLOR "Color"
#define CLASS_FONT "Font"
#define CLASS_FUNCTION "FunctionImpl"
#define CLASS_INSTANCE "InstanceImpl"
#define CLASS_MAP "MapImpl"
#define CLASS_MATERIAL "Material"
#define CLASS_SHADER "Shader"
#define CLASS_STREAM "StreamImpl"
#define CLASS_STRING "StringImpl"

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
	bool IsDark;
	size_t Size;
	struct ObjClass* Class;
	struct Obj* Next;
};
struct ObjString {
	Obj Object;
	size_t Length;
	char* Chars;
};
struct ObjModule {
	Obj Object;
	std::vector<struct ObjFunction*>* Functions;
	std::vector<VMValue>* Locals;
	char* SourceFilename;
};
struct ObjFunction {
	Obj Object;
	Uint8 Arity;
	Uint8 MinArity;
	struct Chunk Chunk;
	ObjModule* Module;
	char* Name;
	struct ObjClass* Class;
	size_t Index;
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
	ValueGetFn PropertyGet;
	ValueSetFn PropertySet;
	ElementGetFn ElementGet;
	ElementSetFn ElementSet;
	ObjClass* Parent;
};
struct ObjInstance {
	Obj Object;
	Table* Fields;
	ValueGetFn PropertyGet;
	ValueSetFn PropertySet;
	ElementGetFn ElementGet;
	ElementSetFn ElementSet;
	ObjectDestructor Destructor;
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
	OrderedHashMap<VMValue>* Values;
	OrderedHashMap<char*>* Keys;
};
struct ObjNamespace {
	Obj Object;
	char* Name;
	Table* Fields;
	bool InUse;
};
struct ObjEnum {
	Obj Object;
	char* Name;
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
ObjString* CopyString(std::string path);
ObjString* CopyString(ObjString* string);
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
const char* GetModuleName(ObjModule* module);

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

struct VMThreadCallback {
	Uint32 ThreadID;
	VMValue Callable;
};

#ifdef VM_DEBUG
enum {
	BREAKPOINT_ONHIT_KEEP,
	BREAKPOINT_ONHIT_DISABLE,
	BREAKPOINT_ONHIT_REMOVE
};

struct VMThreadBreakpoint {
	ObjFunction* Function = nullptr;
	Uint32 CodeOffset = 0;
	bool Enabled = true;
	int OnHit = BREAKPOINT_ONHIT_KEEP;
	int Index = 0;
};
#endif

struct CallFrame {
	ObjFunction* Function;
	Uint8* IP;
	Uint8* IPLast;
	Uint8* IPStart;
	VMValue* Slots;
	Uint8 ArgCount;
	ObjModule* Module;
	std::vector<VMValue>* ModuleLocals;

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
	OP_NOP = 0, // Formerly OP_ERROR
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
	OP_UNUSED_1, // Formerly OP_SUPER
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
	OP_SET_ARGUMENT_SLOT,
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
	OP_NEW_HITBOX,
	// Indirect Addressing
	OP_LOCATION_STACK,
	OP_LOCATION_MODULE_LOCAL,
	OP_LOCATION_GLOBAL,
	OP_LOCATION_PROPERTY,
	OP_LOCATION_SUPER_PROPERTY,
	OP_LOCATION_ELEMENT,
	OP_LOAD_INDIRECT,
	OP_STORE_INDIRECT,

	OP_LAST
};

#endif /* ENGINE_BYTECODE_TYPES_H */
