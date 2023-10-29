#if INTERFACE
need_t BytecodeObject;

#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/IO/Stream.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/Types/Entity.h>

#include <set>

class BytecodeObjectManager {
public:
    static bool                 LoadAllClasses;

    static HashMap<VMValue>*    Globals;
    static HashMap<VMValue>*    Constants;

    static std::set<Obj*>       FreedGlobals;

    static VMThread             Threads[8];
    static Uint32               ThreadCount;

    static vector<ObjFunction*> FunctionList;
    static vector<ObjFunction*> AllFunctionList;

    static HashMap<Bytecode>*   Sources;
    static HashMap<ObjClass*>*  Classes;
    static HashMap<char*>*      Tokens;
    static vector<char*>        TokensList;

    static SDL_mutex*           GlobalLock;
};
#endif

#include <Engine/Bytecode/BytecodeObjectManager.h>
#include <Engine/Bytecode/BytecodeObject.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/TextFormats/XML/XMLParser.h>

#include <Engine/Bytecode/Compiler.h>

bool                 BytecodeObjectManager::LoadAllClasses = false;

VMThread             BytecodeObjectManager::Threads[8];
Uint32               BytecodeObjectManager::ThreadCount = 1;

HashMap<VMValue>*    BytecodeObjectManager::Globals = NULL;
HashMap<VMValue>*    BytecodeObjectManager::Constants = NULL;

std::set<Obj*>       BytecodeObjectManager::FreedGlobals;

vector<ObjFunction*> BytecodeObjectManager::FunctionList;
vector<ObjFunction*> BytecodeObjectManager::AllFunctionList;

HashMap<Bytecode>*   BytecodeObjectManager::Sources = NULL;
HashMap<ObjClass*>*  BytecodeObjectManager::Classes = NULL;
HashMap<char*>*      BytecodeObjectManager::Tokens = NULL;
vector<char*>        BytecodeObjectManager::TokensList;

SDL_mutex*           BytecodeObjectManager::GlobalLock = NULL;

// #define DEBUG_STRESS_GC

PUBLIC STATIC void    BytecodeObjectManager::RequestGarbageCollection() {
#ifndef DEBUG_STRESS_GC
    if (GarbageCollector::GarbageSize > GarbageCollector::NextGC)
#endif
    {
        size_t startSize = GarbageCollector::GarbageSize;

        ForceGarbageCollection();

        // startSize = GarbageCollector::GarbageSize - startSize;
        Log::Print(Log::LOG_INFO, "%04X: Freed garbage from %u to %u (%d), next GC at %d", Scene::Frame, (Uint32)startSize, (Uint32)GarbageCollector::GarbageSize, GarbageCollector::GarbageSize - startSize, GarbageCollector::NextGC);
    }
}
PUBLIC STATIC void    BytecodeObjectManager::ForceGarbageCollection() {
    if (BytecodeObjectManager::Lock()) {
        if (BytecodeObjectManager::ThreadCount > 1) {
            BytecodeObjectManager::Unlock();
            return;
        }

        GarbageCollector::Collect();

        BytecodeObjectManager::Unlock();
    }
}

PUBLIC STATIC void    BytecodeObjectManager::ResetStack() {
    Threads[0].ResetStack();
}

// #region Life Cycle
PUBLIC STATIC void    BytecodeObjectManager::Init() {
    if (Globals == NULL)
        Globals = new HashMap<VMValue>(NULL, 8);
    if (Constants == NULL)
        Constants = new HashMap<VMValue>(NULL, 8);
    if (Sources == NULL)
        Sources = new HashMap<Bytecode>(NULL, 8);
    if (Classes == NULL)
        Classes = new HashMap<ObjClass*>(NULL, 8);
    if (Tokens == NULL)
        Tokens = new HashMap<char*>(NULL, 64);

    memset(VMThread::InstructionIgnoreMap, 0, sizeof(VMThread::InstructionIgnoreMap));

    GlobalLock = SDL_CreateMutex();

    for (Uint32 i = 0; i < sizeof(Threads) / sizeof(VMThread); i++) {
        memset(&Threads[i].Stack, 0, sizeof(Threads[i].Stack));
        memset(&Threads[i].RegisterValue, 0, sizeof(Threads[i].RegisterValue));
        memset(&Threads[i].Frames, 0, sizeof(Threads[i].Frames));
        memset(&Threads[i].Name, 0, sizeof(Threads[i].Name));

        Threads[i].FrameCount = 0;
        Threads[i].ReturnFrame = 0;
        Threads[i].State = 0;
        Threads[i].DebugInfo = false;

        Threads[i].ID = i;
        Threads[i].StackTop = Threads[i].Stack;
    }
    ThreadCount = 1;
}
PUBLIC STATIC void    BytecodeObjectManager::DisposeGlobalValueTable(HashMap<VMValue>* globals) {
    globals->ForAll(FreeGlobalValue);
    globals->Clear();
    delete globals;
}
PUBLIC STATIC void    BytecodeObjectManager::Dispose() {
    // NOTE: Remove GC-able values from these tables so they may be cleaned up.
    if (Globals)
        Globals->ForAll(RemoveNonGlobalableValue);
    if (Constants)
        Constants->ForAll(RemoveNonGlobalableValue);

    Threads[0].FrameCount = 0;
    Threads[0].ResetStack();
    ForceGarbageCollection();

    FreedGlobals.clear();

    if (Globals) {
        Log::Print(Log::LOG_VERBOSE, "Freeing values in Globals list...");
        DisposeGlobalValueTable(Globals);
        Log::Print(Log::LOG_VERBOSE, "Done!");
        Globals = NULL;
    }

    if (Constants) {
        Log::Print(Log::LOG_VERBOSE, "Freeing values in Constants list...");
        DisposeGlobalValueTable(Constants);
        Log::Print(Log::LOG_VERBOSE, "Done!");
        Constants = NULL;
    }

    FreeFunctions();

    if (Sources) {
        Sources->WithAll([](Uint32 hash, Bytecode bytecode) -> void {
            Memory::Free(bytecode.Data);
        });
        Sources->Clear();
        delete Sources;
        Sources = NULL;
    }
    if (Classes) {
        Classes->Clear();
        delete Classes;
        Classes = NULL;
    }
    if (Tokens) {
        for (size_t i = 0, iSz = TokensList.size(); i < iSz; i++) {
            Memory::Free(TokensList[i]);
        }
        TokensList.clear();
        Tokens->Clear();
        delete Tokens;
        Tokens = NULL;
    }

    SDL_DestroyMutex(GlobalLock);
}
PUBLIC STATIC void    BytecodeObjectManager::RemoveNonGlobalableValue(Uint32 hash, VMValue value) {
    if (IS_OBJECT(value)) {
        switch (OBJECT_TYPE(value)) {
            case OBJ_CLASS:
            case OBJ_FUNCTION:
            case OBJ_NATIVE:
                break;
            default:
                if (hash)
                    Globals->Remove(hash);
                break;
        }
    }
}
PUBLIC STATIC void    BytecodeObjectManager::FreeNativeValue(Uint32 hash, VMValue value) {
    if (IS_OBJECT(value)) {
        switch (OBJECT_TYPE(value)) {
            case OBJ_NATIVE:
                FREE_OBJ(AS_OBJECT(value), ObjNative);
                break;
                
            default:
                break;
        }
    }
}
PUBLIC STATIC void    BytecodeObjectManager::FreeFunction(ObjFunction* function) {
    /*
    printf("OBJ_FUNCTION: %p (%s)\n", function,
        function->Name ?
            (function->Name->Chars ? function->Name->Chars : "NULL") :
            "NULL");
    //*/
    if (function->Name != NULL)
        FreeValue(OBJECT_VAL(function->Name));

    for (size_t i = 0; i < function->Chunk.Constants->size(); i++)
        FreeValue((*function->Chunk.Constants)[i]);
    function->Chunk.Constants->clear();

    ChunkFree(&function->Chunk);

    FREE_OBJ(function, ObjFunction);
}
PUBLIC STATIC void    BytecodeObjectManager::FreeClass(ObjClass* klass) {
    // Subfunctions are already freed as a byproduct of the AllFunctionList,
    // so just do natives.
    klass->Methods->ForAll(FreeNativeValue);
    delete klass->Methods;

    // A class does not own its values, so it's not allowed
    // to free them.
    delete klass->Fields;

    if (klass->Name)
        FreeValue(OBJECT_VAL(klass->Name));

    FREE_OBJ(klass, ObjClass);
}
PUBLIC STATIC void    BytecodeObjectManager::FreeString(ObjString* string) {
    if (string->Chars != NULL)
        Memory::Free(string->Chars);
    string->Chars = NULL;

    FREE_OBJ(string, ObjString);
}
PUBLIC STATIC void    BytecodeObjectManager::FreeGlobalValue(Uint32 hash, VMValue value) {
    if (IS_OBJECT(value)) {
        Obj* object = AS_OBJECT(value);
        if (FreedGlobals.find(object) != FreedGlobals.end())
            return;

#ifdef DEBUG_FREE_GLOBALS
        if (Tokens->Get(hash))
            Log::Print(Log::LOG_VERBOSE, "Freeing global %s, type %s", Tokens->Get(hash), GetTypeString(value));
        else
            Log::Print(Log::LOG_VERBOSE, "Freeing global %08X, type %s", hash, GetTypeString(value));
#endif

        switch (OBJECT_TYPE(value)) {
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(value);
                FreeClass(klass);
                FreedGlobals.insert(object);
                break;
            }
            case OBJ_NATIVE: {
                FREE_OBJ(AS_OBJECT(value), ObjNative);
                FreedGlobals.insert(object);
                break;
            }
            default:
                break;
        }
    }
}
PRIVATE STATIC void    BytecodeObjectManager::FreeFunctions() {
    Log::Print(Log::LOG_VERBOSE, "Freeing %d functions...", AllFunctionList.size());
    for (size_t i = 0; i < AllFunctionList.size(); i++) {
        FreeFunction(AllFunctionList[i]);
    }
    AllFunctionList.clear();
    Log::Print(Log::LOG_VERBOSE, "Done!");
}
PUBLIC STATIC void    BytecodeObjectManager::PrintHashTableValues(Uint32 hash, VMValue value) {
    if (IS_OBJECT(value)) {
        switch (OBJECT_TYPE(value)) {
            case OBJ_CLASS: {
                printf("OBJ_CLASS: %p (%s)\n", AS_OBJECT(value), Memory::GetName(AS_OBJECT(value)));
                break;
            }
            case OBJ_FUNCTION: {
                printf("OBJ_FUNCTION: %p (%s)\n", AS_OBJECT(value), Memory::GetName(AS_OBJECT(value)));
                break;
            }
            case OBJ_NATIVE: {
                printf("OBJ_NATIVE: %p (%s)\n", AS_OBJECT(value), Memory::GetName(AS_OBJECT(value)));
                break;
            }
            case OBJ_BOUND_METHOD: {
                printf("OBJ_BOUND_METHOD: %p\n", AS_OBJECT(value));
                break;
            }
            case OBJ_CLOSURE: {
                printf("OBJ_CLOSURE: %p\n", AS_OBJECT(value));
                break;
            }
            case OBJ_INSTANCE: {
                printf("OBJ_INSTANCE: %p\n", AS_OBJECT(value));
                break;
            }
            case OBJ_STRING: {
                printf("OBJ_STRING: %p\n", AS_OBJECT(value));
                break;
            }
            case OBJ_ARRAY: {
                printf("OBJ_ARRAY: %p\n", AS_OBJECT(value));
                break;
            }
            case OBJ_UPVALUE: {
                printf("OBJ_UPVALUE: %p\n", AS_OBJECT(value));
                break;
            }
            default:
                break;
        }
    }
}
// #endregion

// #region ValueFuncs
PUBLIC STATIC VMValue BytecodeObjectManager::CastValueAsString(VMValue v) {
    if (IS_STRING(v))
        return v;

    char* buffer = (char*)malloc(512);
    int   buffer_info[2] = { 0, 512 };
    Compiler::PrintValue(&buffer, buffer_info, v);
    v = OBJECT_VAL(CopyString(buffer, buffer_info[0]));
    free(buffer);
    return v;
}
PUBLIC STATIC VMValue BytecodeObjectManager::CastValueAsInteger(VMValue v) {
    float a;
    switch (v.Type) {
        case VAL_DECIMAL:
        case VAL_LINKED_DECIMAL:
            a = AS_DECIMAL(v);
            return INTEGER_VAL((int)a);
        case VAL_INTEGER:
            return v;
        case VAL_LINKED_INTEGER:
            return INTEGER_VAL(AS_INTEGER(v));
        default:
            // NOTE: Should error here instead.
            break;
    }
    return NULL_VAL;
}
PUBLIC STATIC VMValue BytecodeObjectManager::CastValueAsDecimal(VMValue v) {
    int a;
    switch (v.Type) {
        case VAL_DECIMAL:
            return v;
        case VAL_LINKED_DECIMAL:
            return DECIMAL_VAL(AS_DECIMAL(v));
        case VAL_INTEGER:
        case VAL_LINKED_INTEGER:
            a = AS_INTEGER(v);
            return DECIMAL_VAL((float)a);
        default:
            // NOTE: Should error here instead.
            break;
    }
    return NULL_VAL;
}
PUBLIC STATIC VMValue BytecodeObjectManager::Concatenate(VMValue va, VMValue vb) {
    ObjString* a = AS_STRING(va);
    ObjString* b = AS_STRING(vb);

    size_t length = a->Length + b->Length;
    ObjString* result = AllocString(length);

    memcpy(result->Chars, a->Chars, a->Length);
    memcpy(result->Chars + a->Length, b->Chars, b->Length);
    result->Chars[length] = 0;
    return OBJECT_VAL(result);
}

PUBLIC STATIC bool    BytecodeObjectManager::ValuesSortaEqual(VMValue a, VMValue b) {
    if ((a.Type == VAL_DECIMAL && b.Type == VAL_INTEGER) ||
        (a.Type == VAL_INTEGER && b.Type == VAL_DECIMAL)) {
        float a_d = AS_DECIMAL(CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(CastValueAsDecimal(b));
        return (a_d == b_d);
    }

    if (IS_STRING(a) && IS_STRING(b)) {
        ObjString* astr = AS_STRING(a);
        ObjString* bstr = AS_STRING(b);
        return astr->Length == bstr->Length && !memcmp(astr->Chars, bstr->Chars, astr->Length);
    }

    if (IS_BOUND_METHOD(a) && IS_BOUND_METHOD(b)) {
        ObjBoundMethod* abm = AS_BOUND_METHOD(a);
        ObjBoundMethod* bbm = AS_BOUND_METHOD(b);
        return ValuesEqual(abm->Receiver, bbm->Receiver) && abm->Method == bbm->Method;
    }

    return BytecodeObjectManager::ValuesEqual(a, b);
}
PUBLIC STATIC bool    BytecodeObjectManager::ValuesEqual(VMValue a, VMValue b) {
    if (a.Type == VAL_LINKED_INTEGER) goto SKIP_CHECK;
    if (a.Type == VAL_LINKED_DECIMAL) goto SKIP_CHECK;
    if (b.Type == VAL_LINKED_INTEGER) goto SKIP_CHECK;
    if (b.Type == VAL_LINKED_DECIMAL) goto SKIP_CHECK;

    if (a.Type != b.Type) return false;

    SKIP_CHECK:

    switch (a.Type) {
        case VAL_LINKED_INTEGER:
        case VAL_INTEGER: return AS_INTEGER(a) == AS_INTEGER(b);

        case VAL_LINKED_DECIMAL:
        case VAL_DECIMAL: return AS_DECIMAL(a) == AS_DECIMAL(b);

        case VAL_OBJECT:  return AS_OBJECT(a) == AS_OBJECT(b);

        case VAL_NULL:  return true;
    }
    return false;
}
PUBLIC STATIC bool    BytecodeObjectManager::ValueFalsey(VMValue a) {
    if (a.Type == VAL_NULL) return true;

    switch (a.Type) {
        case VAL_LINKED_INTEGER:
        case VAL_INTEGER: return AS_INTEGER(a) == 0;
        case VAL_LINKED_DECIMAL:
        case VAL_DECIMAL: return AS_DECIMAL(a) == 0.0f;
        case VAL_OBJECT:  return false;
    }
    return false;
}

PUBLIC STATIC VMValue BytecodeObjectManager::DelinkValue(VMValue val) {
    if (IS_LINKED_DECIMAL(val))
        return DECIMAL_VAL(AS_DECIMAL(val));
    if (IS_LINKED_INTEGER(val))
        return INTEGER_VAL(AS_INTEGER(val));

    return val;
}

PUBLIC STATIC void    BytecodeObjectManager::FreeValue(VMValue value) {
    if (IS_OBJECT(value)) {
        Obj* objectPointer = AS_OBJECT(value);
        switch (OBJECT_TYPE(value)) {
            case OBJ_BOUND_METHOD: {
                FREE_OBJ(objectPointer, ObjBoundMethod);
                break;
            }
            case OBJ_INSTANCE: {
                ObjInstance* instance = AS_INSTANCE(value);

                // An instance does not own its values, so it's not allowed
                // to free them.
                delete instance->Fields;

                FREE_OBJ(instance, ObjInstance);
                break;
            }
            case OBJ_STRING: {
                ObjString* string = AS_STRING(value);
                FreeString(string);
                break;
            }
            case OBJ_ARRAY: {
                ObjArray* array = AS_ARRAY(value);

                // An array does not own its values, so it's not allowed
                // to free them.
                array->Values->clear();
                delete array->Values;

                FREE_OBJ(array, ObjArray);
                break;
            }
            case OBJ_MAP: {
                ObjMap* map = AS_MAP(value);

                // Free keys
                map->Keys->WithAll([](Uint32, char* ptr) -> void {
                    free(ptr);
                });

                // Free Keys table
                delete map->Keys;

                // Free Values table
                delete map->Values;

                FREE_OBJ(map, ObjMap);
                break;
            }
            case OBJ_STREAM: {
                ObjStream* stream = AS_STREAM(value);

                if (!stream->Closed)
                    stream->StreamPtr->Close();

                FREE_OBJ(stream, ObjStream);
                break;
            }
            default:
                break;
        }
    }
}
// #endregion

// #region GlobalFuncs
PUBLIC STATIC bool    BytecodeObjectManager::Lock() {
    return (SDL_LockMutex(GlobalLock) == 0);
}
PUBLIC STATIC void    BytecodeObjectManager::Unlock() {
    SDL_UnlockMutex(GlobalLock);
}

PUBLIC STATIC void    BytecodeObjectManager::DefineMethod(int index, Uint32 hash) {
    if ((unsigned)index >= AllFunctionList.size())
        return;

    ObjFunction* function = AllFunctionList[index];
    VMValue methodValue = OBJECT_VAL(function);

    ObjClass* klass = AS_CLASS(Threads[0].Peek(0));
    klass->Methods->Put(hash, methodValue);

    if (hash == klass->Hash)
        klass->Initializer = methodValue;

    function->ClassName = klass->Name;

    Threads[0].Pop();
}
PUBLIC STATIC void    BytecodeObjectManager::DefineNative(ObjClass* klass, const char* name, NativeFn function) {
    if (function == NULL) return;
    if (klass == NULL) return;
    if (name == NULL) return;

    if (!klass->Methods->Exists(name))
        klass->Methods->Put(name, OBJECT_VAL(NewNative(function)));
}
PUBLIC STATIC void    BytecodeObjectManager::GlobalLinkInteger(ObjClass* klass, const char* name, int* value) {
    if (name == NULL) return;

    if (klass == NULL) {
        Globals->Put(name, INTEGER_LINK_VAL(value));
    }
    else {
        klass->Methods->Put(name, INTEGER_LINK_VAL(value));
    }
}
PUBLIC STATIC void    BytecodeObjectManager::GlobalLinkDecimal(ObjClass* klass, const char* name, float* value) {
    if (name == NULL) return;

    if (klass == NULL) {
        Globals->Put(name, DECIMAL_LINK_VAL(value));
    }
    else {
        klass->Methods->Put(name, DECIMAL_LINK_VAL(value));
    }
}
PUBLIC STATIC void    BytecodeObjectManager::GlobalConstInteger(ObjClass* klass, const char* name, int value) {
    if (name == NULL) return;
    if (klass == NULL)
        Constants->Put(name, INTEGER_VAL(value));
    else
        klass->Methods->Put(name, INTEGER_VAL(value));
}
PUBLIC STATIC void    BytecodeObjectManager::GlobalConstDecimal(ObjClass* klass, const char* name, float value) {
    if (name == NULL) return;
    if (klass == NULL)
        Constants->Put(name, DECIMAL_VAL(value));
    else
        klass->Methods->Put(name, DECIMAL_VAL(value));
}
PUBLIC STATIC void    BytecodeObjectManager::SetClassParent(ObjClass* klass) {
    Uint32 shash = klass->ParentHash;
    if (BytecodeObjectManager::Globals->Exists(shash)) {
        VMValue val = BytecodeObjectManager::Globals->Get(shash);
        if (IS_CLASS(val))
            klass->Parent = AS_CLASS(val);
    }
}

PUBLIC STATIC void    BytecodeObjectManager::LinkStandardLibrary() {
    StandardLibrary::Link();
}
PUBLIC STATIC void    BytecodeObjectManager::LinkExtensions() {

}
// #endregion

#define FG_YELLOW ""
#define FG_RESET ""

#if defined(LINUX)
    #undef FG_YELLOW
    #undef FG_RESET
    #define FG_YELLOW "\x1b[1;93m"
    #define FG_RESET "\x1b[m"
#endif

// #region ObjectFuncs
PUBLIC STATIC void    BytecodeObjectManager::RunFromIBC(Bytecode bytecode, Uint32 filenameHash) {
    MemoryStream* stream = MemoryStream::New(bytecode.Data, bytecode.Size);
    if (!stream)
        return;

    FunctionList.clear();

    Uint8 magic[4];
    stream->ReadBytes(magic, 4);
    if (memcmp(Compiler::Magic, magic, 4) != 0) {
        printf("Incorrect magic!\n");
        stream->Close();
        return;
    }

    // Uint8 version = stream->ReadByte();
    stream->Skip(1);
    Uint8 opts = stream->ReadByte();
    stream->Skip(1);
    stream->Skip(1);

    bool doLineNumbers = opts & 1;
    bool hasSourceFilename = opts & 2;

    size_t functionListOffset = AllFunctionList.size();

    int chunkCount = stream->ReadInt32();
    for (int i = 0; i < chunkCount; i++) {
        int   count = stream->ReadInt32();
        int   arity = stream->ReadInt32();
        Uint32 hash = stream->ReadUInt32();

        ObjFunction* function = NewFunction();
        function->Arity = arity;
        function->NameHash = hash;
        function->FunctionListOffset = functionListOffset;
        function->Chunk.Count = count;
        function->Chunk.OwnsMemory = false;

        function->Chunk.Code = stream->pointer;
        stream->Skip(count * sizeof(Uint8));

        if (doLineNumbers) {
            function->Chunk.Lines = (int*)stream->pointer;
            stream->Skip(count * sizeof(int));
        }

        int constantCount = stream->ReadInt32();
        for (int c = 0; c < constantCount; c++) {
            Uint8 type = stream->ReadByte();
            switch (type) {
                case VAL_INTEGER:
                    ChunkAddConstant(&function->Chunk, INTEGER_VAL(stream->ReadInt32()));
                    break;
                case VAL_DECIMAL:
                    ChunkAddConstant(&function->Chunk, DECIMAL_VAL(stream->ReadFloat()));
                    break;
                case VAL_OBJECT:
                    // if (OBJECT_TYPE(constt) == OBJ_STRING) {
                        char* str = stream->ReadString();
                        ChunkAddConstant(&function->Chunk, OBJECT_VAL(CopyString(str)));
                        Memory::Free(str);
                    // }
                    // else {
                        // printf("Unsupported object type...Chief.\n");
                    // }
                    break;
            }
        }

        FunctionList.push_back(function);
        AllFunctionList.push_back(function);
    }

    if (doLineNumbers && Tokens) {
        int tokenCount = stream->ReadInt32();
        for (int t = 0; t < tokenCount; t++) {
            char* string = stream->ReadString();
            Uint32 hash = Murmur::EncryptString(string);

            if (!Tokens->Exists(hash)) {
                Tokens->Put(hash, string);
                TokensList.push_back(string);
            }
            else
                Memory::Free(string);
        }
    }

    if (hasSourceFilename) {
        char* fn = stream->ReadString();
        for (ObjFunction* function : FunctionList)
            StringUtils::Copy(function->SourceFilename, fn, sizeof(function->SourceFilename));
        Memory::Free(fn);
    }
    else {
        char sourceFilename[256];
        snprintf(sourceFilename, sizeof(sourceFilename), "%08X", filenameHash);
        for (ObjFunction* function : FunctionList)
            StringUtils::Copy(function->SourceFilename, sourceFilename, sizeof(function->SourceFilename));
    }

    stream->Close();

    Threads[0].RunFunction(FunctionList[0], 0);
}
PUBLIC STATIC bool    BytecodeObjectManager::CallFunction(char* functionName) {
    if (!Globals->Exists(functionName))
        return false;

    VMValue functionValue = Globals->Get(functionName);
    if (!IS_FUNCTION(functionValue))
        return false;

    ObjFunction* function = AS_FUNCTION(functionValue);
    if (!function) // Does this need to be checked?
        return false;

    Threads[0].RunEntityFunction(function, 0);
    return true;
}
PUBLIC STATIC Entity* BytecodeObjectManager::SpawnObject(const char* objectName) {
    Uint32 hash = Globals->HashFunction(objectName, strlen(objectName));
    ObjClass* klass = AS_CLASS(Globals->Get(hash));
    if (!klass) {
        Log::Print(Log::LOG_ERROR, "No class! Can't find: %s\n", objectName);
        exit(-1);
    }

    BytecodeObject* object = new BytecodeObject;

    ObjInstance* instance = NewInstance(klass);
    object->Link(instance);

    return object;
}
PUBLIC STATIC Bytecode BytecodeObjectManager::GetBytecodeFromFilenameHash(Uint32 filenameHash) {
    if (Sources->Exists(filenameHash))
        return Sources->Get(filenameHash);

    Bytecode bytecode;
    bytecode.Data = nullptr;
    bytecode.Size = 0;

    char filename[64];
    snprintf(filename, sizeof filename, "Objects/%08X.ibc", filenameHash);

    if (!ResourceManager::ResourceExists(filename)) {
        return bytecode;
    }

    ResourceStream* stream = ResourceStream::New(filename);
    if (!stream) {
        // Object doesn't exist?
        return bytecode;
    }

    bytecode.Size = stream->Length();
    bytecode.Data = (Uint8*)Memory::TrackedMalloc("Bytecode::Data", bytecode.Size);
    stream->ReadBytes(bytecode.Data, bytecode.Size);
    stream->Close();

    Sources->Put(filenameHash, bytecode);

    return bytecode;
}
PUBLIC STATIC bool    BytecodeObjectManager::ClassExists(const char* objectName) {
    return SourceFileMap::ClassMap->Exists(objectName);
}
PUBLIC STATIC bool    BytecodeObjectManager::LoadClass(const char* objectName) {
    if (!objectName || !*objectName)
        return false;

    if (!BytecodeObjectManager::ClassExists(objectName)) {
        Log::Print(Log::LOG_VERBOSE, "Could not find classmap for %s%s%s! (Hash: 0x%08X)", FG_YELLOW, objectName, FG_RESET, SourceFileMap::ClassMap->HashFunction(objectName, strlen(objectName)));
        return false;
    }

    // On first load:
    vector<Uint32>* filenameHashList = SourceFileMap::ClassMap->Get(objectName);

    for (size_t fn = 0; fn < filenameHashList->size(); fn++) {
        Uint32 filenameHash = (*filenameHashList)[fn];

        if (!Sources->Exists(filenameHash)) {
            Bytecode bytecode = BytecodeObjectManager::GetBytecodeFromFilenameHash(filenameHash);
            if (!bytecode.Data) {
                Log::Print(Log::LOG_WARN, "Object \"%s\" does not exist!", objectName);
                return false;
            }

            // Load the object class
            if (fn == 0) {
                Log::Print(Log::LOG_VERBOSE, "Loading the object %s%s%s class, %d filenames...",
                    Log::WriteToFile ? "" : FG_YELLOW, objectName, Log::WriteToFile ? "" : FG_RESET,
                    (int)filenameHashList->size());
            }

            RunFromIBC(bytecode, filenameHash);
        }
    }

    // Set native functions for that new object class
    if (!Classes->Exists(objectName)) {
        // Log::Print(Log::LOG_VERBOSE, "Setting native functions for class %s...", objectName);
        ObjClass* klass = AS_CLASS(Globals->Get(objectName));
        if (!klass) {
            Log::Print(Log::LOG_ERROR, "Could not find class of %s", objectName);
            return false;
        }
        if (klass->Extended == 0) {
            BytecodeObjectManager::AddNativeFunctions(klass);
        }
        Classes->Put(objectName, klass);
    }

    return true;
}
PUBLIC STATIC void   BytecodeObjectManager::AddNativeFunctions(ObjClass* klass) {
#define DEF_NATIVE(name) BytecodeObjectManager::DefineNative(klass, #name, BytecodeObject::VM_##name)
    DEF_NATIVE(InView);
    DEF_NATIVE(Animate);
    DEF_NATIVE(ApplyPhysics);
    DEF_NATIVE(SetAnimation);
    DEF_NATIVE(ResetAnimation);
    DEF_NATIVE(GetHitboxFromSprite);
    DEF_NATIVE(ReturnHitboxFromSprite);
    DEF_NATIVE(AddToRegistry);
    DEF_NATIVE(RemoveFromRegistry);
    DEF_NATIVE(CollidedWithObject);
    DEF_NATIVE(CollideWithObject);
    DEF_NATIVE(SolidCollideWithObject);
    DEF_NATIVE(TopSolidCollideWithObject);
    DEF_NATIVE(PropertyGet);
    DEF_NATIVE(PropertyExists);
    DEF_NATIVE(SetViewVisibility);
    DEF_NATIVE(SetViewOverride);
    DEF_NATIVE(AddToDrawGroup);
    DEF_NATIVE(IsInDrawGroup);
    DEF_NATIVE(RemoveFromDrawGroup);
    DEF_NATIVE(PlaySound);
    DEF_NATIVE(LoopSound);
    DEF_NATIVE(StopSound);
    DEF_NATIVE(StopAllSounds);
#undef DEF_NATIVE
}
PUBLIC STATIC Entity* BytecodeObjectManager::SpawnFunction(const char* objectName) {
    return BytecodeObjectManager::SpawnObject(objectName);
}
PUBLIC STATIC void    BytecodeObjectManager::LoadClasses() {
    SourceFileMap::ClassMap->ForAll([](Uint32, vector<Uint32>* filenameHashList) -> void {
        for (size_t fn = 0; fn < filenameHashList->size(); fn++) {
            Uint32 filenameHash = (*filenameHashList)[fn];

            Bytecode bytecode = BytecodeObjectManager::GetBytecodeFromFilenameHash(filenameHash);
            if (!bytecode.Data) {
                Log::Print(Log::LOG_WARN, "Class %08X does not exist!", filenameHash);
                continue;
            }

            RunFromIBC(bytecode, filenameHash);
        }
    });

    BytecodeObjectManager::ForceGarbageCollection();
}
// #endregion
