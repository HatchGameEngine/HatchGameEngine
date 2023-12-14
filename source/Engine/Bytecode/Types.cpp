#include <Engine/Bytecode/Types.h>

#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/TypeImpl/ArrayImpl.h>
#include <Engine/Bytecode/TypeImpl/MapImpl.h>
#include <Engine/Bytecode/TypeImpl/FunctionImpl.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Hashing/FNV1A.h>

#define ALLOCATE_OBJ(type, objectType) \
    (type*)AllocateObject(sizeof(type), objectType)
#define ALLOCATE(type, size) \
    (type*)Memory::TrackedMalloc(#type, sizeof(type) * size)

#define GROW_CAPACITY(val) ((val) < 8 ? 8 : val * 2)

static Obj*       AllocateObject(size_t size, ObjType type) {
    // Only do this when allocating more memory
    GarbageCollector::GarbageSize += size;

    Obj* object = (Obj*)Memory::TrackedMalloc("AllocateObject", size);
    object->Type = type;
    object->Class = nullptr;
    object->IsDark = false;
    object->Next = GarbageCollector::RootObject;
    GarbageCollector::RootObject = object;

    return object;
}
static ObjString* AllocateString(char* chars, size_t length, Uint32 hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    Memory::Track(string, "NewString");
    string->Length = length;
    string->Chars = chars;
    string->Hash = hash;
    return string;
}

ObjString*        TakeString(char* chars, size_t length) {
    Uint32 hash = FNV1A::EncryptData(chars, length);
    return AllocateString(chars, length, hash);
}
ObjString*        TakeString(char* chars) {
    return TakeString(chars, strlen(chars));
}
ObjString*        CopyString(const char* chars, size_t length) {
    Uint32 hash = FNV1A::EncryptData(chars, length);

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return AllocateString(heapChars, length, hash);
}
ObjString*        CopyString(const char* chars) {
    return CopyString(chars, strlen(chars));
}
ObjString*        AllocString(size_t length) {
    char* heapChars = ALLOCATE(char, length + 1);
    heapChars[length] = '\0';

    return AllocateString(heapChars, length, 0x00000000);
}

ObjFunction*      NewFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    Memory::Track(function, "NewFunction");
    function->Object.Class = FunctionImpl::Class;
    function->Arity = 0;
    function->UpvalueCount = 0;
    function->Name = NULL;
    function->ClassName = NULL;
    function->SourceFilename = NULL;
    function->Chunk.Init();
    return function;
}
ObjNative*        NewNative(NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    Memory::Track(native, "NewNative");
    native->Function = function;
    return native;
}
ObjUpvalue*       NewUpvalue(VMValue* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->Closed = NULL_VAL;
    upvalue->Value = slot;
    upvalue->Next = NULL;
    return upvalue;
}
ObjClosure*       NewClosure(ObjFunction* function) {
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
ObjClass*         NewClass(Uint32 hash) {
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    Memory::Track(klass, "NewClass");
    klass->Name = NULL;
    klass->Hash = hash;
    klass->Methods = new Table(NULL, 4);
    klass->Fields = new Table(NULL, 16);
    klass->Initializer = NULL_VAL;
    klass->Type = CLASS_TYPE_NORMAL;
    klass->ParentHash = 0;
    klass->Parent = NULL;
    return klass;
}
ObjInstance*      NewInstance(ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    Memory::Track(instance, "NewInstance");
    instance->Object.Class = klass;
    instance->Fields = new Table(NULL, 16);
    instance->EntityPtr = NULL;
    return instance;
}
ObjBoundMethod*   NewBoundMethod(VMValue receiver, ObjFunction* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
    Memory::Track(bound, "NewBoundMethod");
    bound->Receiver = receiver;
    bound->Method = method;
    return bound;
}
ObjArray*         NewArray() {
    ObjArray* array = ALLOCATE_OBJ(ObjArray, OBJ_ARRAY);
    Memory::Track(array, "NewArray");
    array->Object.Class = ArrayImpl::Class;
    array->Values = new vector<VMValue>();
    return array;
}
ObjMap*           NewMap() {
    ObjMap* map = ALLOCATE_OBJ(ObjMap, OBJ_MAP);
    Memory::Track(map, "NewMap");
    map->Object.Class = MapImpl::Class;
    map->Values = new HashMap<VMValue>(NULL, 4);
    map->Keys = new HashMap<char*>(NULL, 4);
    return map;
}
ObjStream*        NewStream(Stream* streamPtr, bool writable) {
    ObjStream* stream = ALLOCATE_OBJ(ObjStream, OBJ_STREAM);
    Memory::Track(stream, "NewStream");
    stream->StreamPtr = streamPtr;
    stream->Writable = writable;
    stream->Closed = false;
    return stream;
}
ObjNamespace*     NewNamespace(Uint32 hash) {
    ObjNamespace* ns = ALLOCATE_OBJ(ObjNamespace, OBJ_NAMESPACE);
    Memory::Track(ns, "NewNamespace");
    ns->Name = NULL;
    ns->Hash = hash;
    ns->Fields = new Table(NULL, 16);
    return ns;
}
ObjEnum*          NewEnumeration(Uint32 hash) {
    ObjEnum* enumeration = ALLOCATE_OBJ(ObjEnum, OBJ_ENUM);
    Memory::Track(enumeration, "NewEnumeration");
    enumeration->Name = NULL;
    enumeration->Hash = hash;
    enumeration->Fields = new Table(NULL, 16);
    return enumeration;
}

bool              ValuesEqual(VMValue a, VMValue b) {
    if (a.Type != b.Type) return false;

    switch (a.Type) {
        case VAL_INTEGER: return AS_INTEGER(a) == AS_INTEGER(b);
        case VAL_DECIMAL: return AS_DECIMAL(a) == AS_DECIMAL(b);
        case VAL_OBJECT:  return AS_OBJECT(a) == AS_OBJECT(b);
    }
    return false;
}

const char*       GetTypeString(Uint32 type) {
    switch (type) {
        case VAL_NULL:
            return "Null";
        case VAL_INTEGER:
        case VAL_LINKED_INTEGER:
            return "Integer";
        case VAL_DECIMAL:
        case VAL_LINKED_DECIMAL:
            return "Decimal";
        case VAL_OBJECT:
            return "Object";
    }
    return "Unknown Type";
}
const char*       GetObjectTypeString(Uint32 type) {
    switch (type) {
        case OBJ_BOUND_METHOD:
            return "Bound Method";
        case OBJ_FUNCTION:
            return "Function";
        case OBJ_CLASS:
            return "Class";
        case OBJ_ENUM:
            return "Enumeration";
        case OBJ_CLOSURE:
            return "Closure";
        case OBJ_INSTANCE:
            return "Instance";
        case OBJ_NATIVE:
            return "Native";
        case OBJ_STRING:
            return "String";
        case OBJ_UPVALUE:
            return "Upvalue";
        case OBJ_ARRAY:
            return "Array";
        case OBJ_MAP:
            return "Map";
        case OBJ_STREAM:
            return "Stream";
        case OBJ_NAMESPACE:
            return "Namespace";
    }
    return "Unknown Object Type";
}
const char*       GetValueTypeString(VMValue value) {
    if (value.Type == VAL_OBJECT)
        return GetObjectTypeString(OBJECT_TYPE(value));
    else
        return GetTypeString(value.Type);
}

void              Chunk::Init() {
    Count = 0;
    Capacity = 0;
    Code = NULL;
    Lines = NULL;
    Constants = new vector<VMValue>();
}
void              Chunk::Alloc() {
    if (!Code)
        Code = (Uint8*)Memory::TrackedMalloc("Chunk::Code", sizeof(Uint8) * Capacity);
    else
        Code = (Uint8*)Memory::Realloc(Code, sizeof(Uint8) * Capacity);

    if (!Lines)
        Lines = (int*)Memory::TrackedMalloc("Chunk::Lines", sizeof(int) * Capacity);
    else
        Lines = (int*)Memory::Realloc(Lines, sizeof(int) * Capacity);

    OwnsMemory = true;
}
void              Chunk::Free() {
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
}
void              Chunk::Write(Uint8 byte, int line) {
    if (Capacity < Count + 1) {
        int oldCapacity = Capacity;
        Capacity = GROW_CAPACITY(oldCapacity);
        Alloc();
    }
    Code[Count] = byte;
    Lines[Count] = line;
    Count++;
}
int               Chunk::AddConstant(VMValue value) {
    Constants->push_back(value);
    return (int)Constants->size() - 1;
}
