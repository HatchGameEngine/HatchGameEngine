#include <Engine/Bytecode/GarbageCollector.h>

#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Includes/HashMap.h>

#ifdef USE_CLOCK
#include <Engine/Diagnostics/Clock.h>
#endif

#ifndef HSL_STANDALONE
#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Scene.h>
#endif

GarbageCollector::GarbageCollector(ScriptManager* manager) {
	Manager = manager;
	NextGC = 0x100000;
	GarbageSize = 0;
}

void GarbageCollector::Collect(bool doLog) {
	// Nothing to do
	if (!Manager->RootObject) {
		return;
	}

#ifdef USE_CLOCK
	double grayElapsed = Clock::GetTicks();
#endif

	// Mark threads (should lock here for safety)
	for (Uint32 t = 0; t < MAX_VM_THREADS; t++) {
		VMThread* thread = &Manager->Threads[t];
		if (thread->Active) {
			// Mark stack roots
			for (VMValue* slot = thread->Stack; slot < thread->StackTop; slot++) {
				GrayValue(*slot);
			}
			// Mark frame modules
			for (Uint32 i = 0; i < thread->FrameCount; i++) {
				GrayObject(thread->Frames[i].Module);
			}
		}
	}

	// Mark global roots
	GrayHashMap(Manager->Globals);

	// Mark constants
	GrayHashMap(Manager->Constants);

#ifndef HSL_STANDALONE
	// Mark objects
	for (Entity* ent = Scene::ObjectFirst; ent; ent = ent->NextSceneEntity) {
		ScriptEntity* scriptEntity = (ScriptEntity*)ent;
		GrayObject(scriptEntity->Instance);
	}
#endif

	// Mark modules
	for (size_t i = 0; i < Manager->ModuleList.size(); i++) {
		GrayObject(Manager->ModuleList[i]);
	}

	// Mark classes
	if (Manager->ImplClasses) {
		Manager->ImplClasses->WithAll([this](Uint32 hash, ObjClass* klass) {
			GrayObject((Obj*)klass);
		});
	}

#ifndef HSL_STANDALONE
	// Mark resources
	CollectResources();
#endif

#ifdef USE_CLOCK
	grayElapsed = Clock::GetTicks() - grayElapsed;

	double blackenElapsed = Clock::GetTicks();
#endif

	// Traverse references
	for (size_t i = 0; i < GrayList.size(); i++) {
		BlackenObject(GrayList[i]);
	}

#ifdef USE_CLOCK
	blackenElapsed = Clock::GetTicks() - blackenElapsed;

	double freeElapsed = Clock::GetTicks();
#endif

	int objectTypeFreed[MAX_OBJ_TYPE] = {0};
	int objectTypeCounts[MAX_OBJ_TYPE] = {0};

	// Collect the white objects
	Obj** object = &Manager->RootObject;
	while (*object != NULL) {
		objectTypeCounts[(*object)->Type]++;

		if (GraySet.count(*object) == 0) {
			objectTypeFreed[(*object)->Type]++;

			// This object wasn't reached, so remove it from the list and free it.
			Obj* unreached = *object;
			*object = unreached->Next;

			FreeObject(unreached);
		}
		else {
			// This object was reached, so move on to the next.
			object = &(*object)->Next;
		}
	}

#ifdef USE_CLOCK
	freeElapsed = Clock::GetTicks() - freeElapsed;
#endif

	if (doLog) {
#ifdef USE_CLOCK
		Log::Print(Log::LOG_VERBOSE, "Sweep: Graying took %.1f ms", grayElapsed);
		Log::Print(Log::LOG_VERBOSE, "Sweep: Blackening took %.1f ms", blackenElapsed);
		Log::Print(Log::LOG_VERBOSE, "Sweep: Freeing took %.1f ms", freeElapsed);
#endif

		for (size_t i = 0; i < MAX_OBJ_TYPE; i++) {
			if (objectTypeFreed[i] && objectTypeCounts[i]) {
				Log::Print(Log::LOG_VERBOSE,
					"Freed %d %s objects out of %d.",
					objectTypeFreed[i],
					GetObjectTypeString(i),
					objectTypeCounts[i]);
			}
		}
	}

	NextGC = GarbageSize + (1024 * 1024);

	GrayList.clear();
	GraySet.clear();
}

#ifndef HSL_STANDALONE
void GarbageCollector::CollectResources() {
	// Mark model materials
	for (size_t i = 0; i < Material::List.size(); i++) {
		GrayObject(Material::List[i]->Object);
	}
}
#endif

void GarbageCollector::FreeObject(Obj* object) {
	Manager->DestroyObject(object);
}

void GarbageCollector::GrayValue(VMValue value) {
	if (IS_OBJECT(value)) {
		GrayObject(AS_OBJECT(value));
	}
}
void GarbageCollector::GrayObject(void* obj) {
	if (obj == NULL) {
		return;
	}

	Obj* object = (Obj*)obj;
	if (GraySet.count(object) == 0) {
		GrayList.push_back(object);
		GraySet.insert(object);
	}
}
void GarbageCollector::GrayHashMap(void* pointer) {
	if (!pointer) {
		return;
	}

	HashMap<VMValue>* hashMap = (HashMap<VMValue>*)pointer;
	hashMap->WithAll([this, hashMap](Uint32, VMValue value) -> void {
		GrayValue(value);
	});
}

void GarbageCollector::BlackenObject(Obj* object) {
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
		GrayHashMap(klass->Methods);
		GrayHashMap(klass->Fields);
		GrayValue(klass->Initializer);
		GrayObject(klass->Parent);
		break;
	}
	case OBJ_ENUM: {
		ObjEnum* enumeration = (ObjEnum*)object;
		GrayHashMap(enumeration->Fields);
		break;
	}
	case OBJ_NAMESPACE: {
		ObjNamespace* ns = (ObjNamespace*)object;
		GrayHashMap(ns->Fields);
		break;
	}
	case OBJ_FUNCTION: {
		ObjFunction* function = (ObjFunction*)object;
		GrayObject(function->Class);
		for (size_t i = 0; i < function->Chunk.Constants->size(); i++) {
			GrayValue((*function->Chunk.Constants)[i]);
		}
		break;
	}
	case OBJ_MODULE: {
		ObjModule* module = (ObjModule*)object;
		for (size_t i = 0; i < module->Locals->size(); i++) {
			GrayValue((*module->Locals)[i]);
		}
		for (size_t fn = 0; fn < module->Functions->size(); fn++) {
			GrayObject((*module->Functions)[fn]);
		}
		break;
	}
	case OBJ_INSTANCE:
	case OBJ_NATIVE_INSTANCE: {
		ObjInstance* instance = (ObjInstance*)object;
		GrayHashMap(instance->Fields);
		break;
	}
#ifndef HSL_STANDALONE
	case OBJ_ENTITY: {
		ObjEntity* entity = (ObjEntity*)object;
		if (entity->EntityPtr) {
			((ScriptEntity*)entity->EntityPtr)->MarkForGarbageCollection();
		}
		break;
	}
#endif
	case OBJ_ARRAY: {
		ObjArray* array = (ObjArray*)object;
		for (size_t i = 0; i < array->Values->size(); i++) {
			GrayValue((*array->Values)[i]);
		}
		break;
	}
	case OBJ_MAP: {
		ObjMap* map = (ObjMap*)object;
		map->Values->WithAll([this](Uint32, VMValue v) -> void {
			GrayValue(v);
		});
		break;
	}
	default:
		// No references
		break;
	}
}
