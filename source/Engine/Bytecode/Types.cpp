#include <Engine/Bytecode/Types.h>

#include <Engine/Bytecode/BytecodeObjectManager.h>
#include <Engine/Bytecode/GarbageCollector.h>
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

char*             HeapCopyString(const char* str, size_t len) {
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, str, len);
    out[len] = 0;
    return out;
}

ObjFunction*      NewFunction() {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    Memory::Track(function, "NewFunction");
    function->Arity = 0;
    function->UpvalueCount = 0;
    function->Name = NULL;
    function->ClassName = NULL;
    ChunkInit(&function->Chunk);
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
    klass->Extended = false;
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
    array->Values = new vector<VMValue>();
    return array;
}
ObjMap*           NewMap() {
    ObjMap* map = ALLOCATE_OBJ(ObjMap, OBJ_MAP);
    Memory::Track(map, "NewMap");
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

bool              ValuesEqual(VMValue a, VMValue b) {
    if (a.Type != b.Type) return false;

    switch (a.Type) {
        case VAL_INTEGER: return AS_INTEGER(a) == AS_INTEGER(b);
        case VAL_DECIMAL: return AS_DECIMAL(a) == AS_DECIMAL(b);
        case VAL_OBJECT:  return AS_OBJECT(a) == AS_OBJECT(b);
    }
    return false;
}

const char*       GetTypeString(VMValue value) {
    switch (value.Type) {
        case VAL_NULL:
            return "Null";
        case VAL_INTEGER:
        case VAL_LINKED_INTEGER:
            return "Integer";
        case VAL_DECIMAL:
        case VAL_LINKED_DECIMAL:
            return "Decimal";
        case VAL_OBJECT:
            switch (OBJECT_TYPE(value)) {
                case OBJ_BOUND_METHOD:
                case OBJ_FUNCTION:
                    return "Event";
                case OBJ_CLASS:
                    return "Class";
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
                default:
                    return "Unknown Object Type";
            }
            break;
    }
    return "Unknown Type";
}

void              ChunkInit(Chunk* chunk) {
    chunk->Count = 0;
    chunk->Capacity = 0;
    chunk->Code = NULL;
    chunk->Lines = NULL;
    chunk->Constants = new vector<VMValue>();
}
void              ChunkAlloc(Chunk* chunk) {
    if (!chunk->Code)
        chunk->Code = (Uint8*)Memory::TrackedMalloc("Chunk::Code", sizeof(Uint8) * chunk->Capacity);
    else
        chunk->Code = (Uint8*)Memory::Realloc(chunk->Code, sizeof(Uint8) * chunk->Capacity);

    if (!chunk->Lines)
        chunk->Lines = (int*)Memory::TrackedMalloc("Chunk::Lines", sizeof(int) * chunk->Capacity);
    else
        chunk->Lines = (int*)Memory::Realloc(chunk->Lines, sizeof(int) * chunk->Capacity);

    chunk->OwnsMemory = true;
}
void              ChunkFree(Chunk* chunk) {
    if (chunk->OwnsMemory) {
        if (chunk->Code) {
            Memory::Free(chunk->Code);
            chunk->Code = NULL;
            chunk->Count = 0;
            chunk->Capacity = 0;
        }
        if (chunk->Lines) {
            Memory::Free(chunk->Lines);
            chunk->Lines = NULL;
        }
    }

    if (chunk->Constants) {
        chunk->Constants->clear();
        chunk->Constants->shrink_to_fit();
        delete chunk->Constants;
    }
}
void              ChunkWrite(Chunk* chunk, Uint8 byte, int line) {
    if (chunk->Capacity < chunk->Count + 1) {
        int oldCapacity = chunk->Capacity;
        chunk->Capacity = GROW_CAPACITY(oldCapacity);
        ChunkAlloc(chunk);
    }
    chunk->Code[chunk->Count] = byte;
    chunk->Lines[chunk->Count] = line;
    chunk->Count++;
}
int               ChunkAddConstant(Chunk* chunk, VMValue value) {
    // BytecodeObjectManager::Push(value);
    chunk->Constants->push_back(value);
    // BytecodeObjectManager::Pop();
    return (int)chunk->Constants->size() - 1;
}
