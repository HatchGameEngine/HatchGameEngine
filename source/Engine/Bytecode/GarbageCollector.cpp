#if INTERFACE
#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/HashMap.h>

class GarbageCollector {
public:
    static vector<Obj*> GrayList;
    static Obj*         RootObject;

    static size_t       NextGC;
    static size_t       GarbageSize;
    static double       MaxTimeAlotted;

    static bool         Print;
    static bool         FilterSweepEnabled;
    static int          FilterSweepType;
};
#endif

#include <Engine/Bytecode/GarbageCollector.h>

#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Scene.h>

#define GC_HEAP_GROW_FACTOR 2

vector<Obj*> GarbageCollector::GrayList;
Obj*         GarbageCollector::RootObject;

size_t       GarbageCollector::NextGC = 1024;
size_t       GarbageCollector::GarbageSize = 0;
double       GarbageCollector::MaxTimeAlotted = 1.0; // 1ms

bool         GarbageCollector::Print = false;
bool         GarbageCollector::FilterSweepEnabled = false;
int          GarbageCollector::FilterSweepType = 0;

PUBLIC STATIC void GarbageCollector::Init() {
    GarbageCollector::RootObject = NULL;
    GarbageCollector::NextGC = 0x100000;
}

PUBLIC STATIC void GarbageCollector::Collect() {
    GrayList.clear();

    double grayElapsed = Clock::GetTicks();

    // Mark threads (should lock here for safety)
    for (Uint32 t = 0; t < ScriptManager::ThreadCount; t++) {
        VMThread* thread = ScriptManager::Threads + t;
        // Mark stack roots
        for (VMValue* slot = thread->Stack; slot < thread->StackTop; slot++) {
            GrayValue(*slot);
        }
        // Mark frame functions
        for (Uint32 i = 0; i < thread->FrameCount; i++) {
            GrayObject(thread->Frames[i].Function);
        }
    }

    // Mark global roots
    GrayHashMap(ScriptManager::Globals);

    // Mark constants
    GrayHashMap(ScriptManager::Constants);

    // Mark static objects
    for (Entity* ent = Scene::StaticObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;

        ScriptEntity* bobj = (ScriptEntity*)ent;
        GrayObject(bobj->Instance);
        GrayHashMap(bobj->Properties);
    }
    // Mark dynamic objects
    for (Entity* ent = Scene::DynamicObjectFirst, *next; ent; ent = next) {
        next = ent->NextEntity;

        ScriptEntity* bobj = (ScriptEntity*)ent;
        GrayObject(bobj->Instance);
        GrayHashMap(bobj->Properties);
    }

    // Mark Scene properties
    if (Scene::Properties)
        GrayHashMap(Scene::Properties);

    // Mark Layer properties
    for (size_t i = 0; i < Scene::Layers.size(); i++) {
        if (Scene::Layers[i].Properties)
            GrayHashMap(Scene::Layers[i].Properties);
    }

    // Mark functions
    for (size_t i = 0; i < ScriptManager::AllFunctionList.size(); i++) {
        GrayObject(ScriptManager::AllFunctionList[i]);
    }

    // Mark classes
    for (size_t i = 0; i < ScriptManager::ClassImplList.size(); i++) {
        GrayObject(ScriptManager::ClassImplList[i]);
    }

    grayElapsed = Clock::GetTicks() - grayElapsed;

    double blackenElapsed = Clock::GetTicks();

    // Traverse references
    for (size_t i = 0; i < GrayList.size(); i++) {
        BlackenObject(GrayList[i]);
    }

    blackenElapsed = Clock::GetTicks() - blackenElapsed;

    double freeElapsed = Clock::GetTicks();

    int objectTypeFreed[MAX_OBJ_TYPE] = { 0 };
    int objectTypeCounts[MAX_OBJ_TYPE] = { 0 };

    // Collect the white objects
    Obj** object = &GarbageCollector::RootObject;
    while (*object != NULL) {
        objectTypeCounts[(*object)->Type]++;

        if (!((*object)->IsDark)) {
            objectTypeFreed[(*object)->Type]++;

            // This object wasn't reached, so remove it from the list and
            // free it.
            Obj* unreached = *object;
            *object = unreached->Next;

            GarbageCollector::FreeValue(OBJECT_VAL(unreached));
        }
        else {
            // This object was reached, so unmark it (for the next GC) and
            // move on to the next.
            (*object)->IsDark = false;
            object = &(*object)->Next;
        }
    }

    freeElapsed = Clock::GetTicks() - freeElapsed;

    Log::Print(Log::LOG_VERBOSE, "Sweep: Graying took %.1f ms", grayElapsed);
    Log::Print(Log::LOG_VERBOSE, "Sweep: Blackening took %.1f ms", blackenElapsed);
    Log::Print(Log::LOG_VERBOSE, "Sweep: Freeing took %.1f ms", freeElapsed);

#define LOG_ME(type) \
    if (objectTypeCounts[type]) \
        Log::Print(Log::LOG_VERBOSE, "Freed %d " #type " objects out of %d.", objectTypeFreed[type], objectTypeCounts[type])

    LOG_ME(OBJ_BOUND_METHOD);
    LOG_ME(OBJ_CLASS);
    LOG_ME(OBJ_FUNCTION);
    LOG_ME(OBJ_INSTANCE);
    LOG_ME(OBJ_ARRAY);
    LOG_ME(OBJ_MAP);
    LOG_ME(OBJ_NATIVE);
    LOG_ME(OBJ_STRING);
    LOG_ME(OBJ_UPVALUE);
    LOG_ME(OBJ_STREAM);
    LOG_ME(OBJ_NAMESPACE);
    LOG_ME(OBJ_ENUM);

#undef LOG_ME

    GarbageCollector::NextGC = GarbageCollector::GarbageSize + (1024 * 1024);
}

PRIVATE STATIC void GarbageCollector::FreeValue(VMValue value) {
    if (!IS_OBJECT(value)) return;

    // If this object is an instance associated with an entity,
    // then delete the latter
    if (OBJECT_TYPE(value) == OBJ_INSTANCE) {
        ObjInstance* instance = AS_INSTANCE(value);
        if (instance->EntityPtr)
            Scene::DeleteRemoved((Entity*)instance->EntityPtr);
    }

    ScriptManager::FreeValue(value);
}

PRIVATE STATIC void GarbageCollector::GrayValue(VMValue value) {
    if (!IS_OBJECT(value)) return;
    GrayObject(AS_OBJECT(value));
}
PRIVATE STATIC void GarbageCollector::GrayObject(void* obj) {
    if (obj == NULL) return;

    Obj* object = (Obj*)obj;
    if (object->IsDark) return;

    object->IsDark = true;

    GrayList.push_back(object);
}
PRIVATE STATIC void GarbageCollector::GrayHashMapItem(Uint32, VMValue value) {
    GrayValue(value);
}
PRIVATE STATIC void GarbageCollector::GrayHashMap(void* pointer) {
    if (!pointer) return;
    ((HashMap<VMValue>*)pointer)->ForAll(GrayHashMapItem);
}

PRIVATE STATIC void GarbageCollector::BlackenObject(Obj* object) {
    GrayObject(object->Class);

    switch (object->Type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            GrayValue(bound->Receiver);
            GrayObject(bound->Method);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            GrayObject(klass->Name);
            GrayHashMap(klass->Methods);
            GrayHashMap(klass->Fields);
            break;
        }
        case OBJ_ENUM: {
            ObjEnum* enumeration = (ObjEnum*)object;
            GrayObject(enumeration->Name);
            GrayHashMap(enumeration->Fields);
            break;
        }
        case OBJ_NAMESPACE: {
            ObjNamespace* ns = (ObjNamespace*)object;
            GrayObject(ns->Name);
            GrayHashMap(ns->Fields);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            GrayObject(function->Name);
            GrayObject(function->ClassName);
            GrayObject(function->SourceFilename);
            for (size_t i = 0; i < function->Chunk.Constants->size(); i++) {
                GrayValue((*function->Chunk.Constants)[i]);
            }
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            GrayHashMap(instance->Fields);
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            for (size_t i = 0; i < array->Values->size(); i++) {
                GrayValue((*array->Values)[i]);
            }
            break;
        }
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            map->Values->ForAll([](Uint32, VMValue v) -> void {
                GrayValue(v);
            });
            break;
        }
        // case OBJ_NATIVE:
        // case OBJ_STRING:
        default:
            // No references
            break;
    }
}

PRIVATE STATIC void GarbageCollector::RemoveWhiteHashMapItem(Uint32, VMValue value) {
    // seems in the craftinginterpreters book this removes the ObjString used
    // for hashing, but we don't use that, so...
}
PRIVATE STATIC void GarbageCollector::RemoveWhiteHashMap(void* pointer) {
    if (!pointer) return;
    ((HashMap<VMValue>*)pointer)->ForAll(RemoveWhiteHashMapItem);
}
