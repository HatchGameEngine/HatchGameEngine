#include <Engine/Bytecode/GarbageCollector.h>

#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Scene.h>

#define GC_HEAP_GROW_FACTOR 2

vector<Obj*> GarbageCollector::GrayList;
Obj* GarbageCollector::RootObject;

size_t GarbageCollector::NextGC = 1024;
size_t GarbageCollector::GarbageSize = 0;
double GarbageCollector::MaxTimeAlotted = 1.0; // 1ms

bool GarbageCollector::Print = false;
bool GarbageCollector::FilterSweepEnabled = false;
int GarbageCollector::FilterSweepType = 0;

void GarbageCollector::Init() {
	GarbageCollector::RootObject = NULL;
	GarbageCollector::NextGC = 0x100000;
}

void GarbageCollector::Collect() {
	// Nothing to do
	if (!RootObject) {
		return;
	}

	GrayList.clear();

	double grayElapsed = Clock::GetTicks();

	// Mark threads (should lock here for safety)
	for (Uint32 t = 0; t < ScriptManager::ThreadCount; t++) {
		VMThread* thread = ScriptManager::Threads + t;
		// Mark stack roots
		for (VMValue* slot = thread->Stack; slot < thread->StackTop; slot++) {
			GrayValue(*slot);
		}
		// Mark frame modules
		for (Uint32 i = 0; i < thread->FrameCount; i++) {
			GrayObject(thread->Frames[i].Module);
		}
	}

	// Mark global roots
	GrayHashMap(ScriptManager::Globals);

	// Mark constants
	GrayHashMap(ScriptManager::Constants);

	// Mark objects
	for (Entity* ent = Scene::ObjectFirst; ent; ent = ent->NextSceneEntity) {
		ScriptEntity* bobj = (ScriptEntity*)ent;
		GrayObject(bobj->Instance);
		GrayHashMap(bobj->Properties);
	}

	// Mark Scene properties
	if (Scene::Properties) {
		GrayHashMap(Scene::Properties);
	}

	// Mark Layer properties
	for (size_t i = 0; i < Scene::Layers.size(); i++) {
		if (Scene::Layers[i].Properties) {
			GrayHashMap(Scene::Layers[i].Properties);
		}
	}

	// Mark modules
	for (size_t i = 0; i < ScriptManager::ModuleList.size(); i++) {
		GrayObject(ScriptManager::ModuleList[i]);
	}

	// Mark classes
	for (size_t i = 0; i < ScriptManager::ClassImplList.size(); i++) {
		GrayObject(ScriptManager::ClassImplList[i]);
	}

	// Mark resources
	GrayInstanceables();

	grayElapsed = Clock::GetTicks() - grayElapsed;

	double blackenElapsed = Clock::GetTicks();

	// Traverse references
	for (size_t i = 0; i < GrayList.size(); i++) {
		BlackenObject(GrayList[i]);
	}

	blackenElapsed = Clock::GetTicks() - blackenElapsed;

	double freeElapsed = Clock::GetTicks();

	int objectTypeFreed[MAX_OBJ_TYPE] = {0};
	int objectTypeCounts[MAX_OBJ_TYPE] = {0};

	// Collect the white objects
	Obj** object = &GarbageCollector::RootObject;
	while (*object != NULL) {
		objectTypeCounts[(*object)->Type]++;

		if (!((*object)->IsDark)) {
			objectTypeFreed[(*object)->Type]++;

			// This object wasn't reached, so remove it
			// from the list and free it.
			Obj* unreached = *object;
			*object = unreached->Next;

			GarbageCollector::FreeObject(unreached);
		}
		else {
			// This object was reached, so unmark it (for
			// the next GC) and move on to the next.
			(*object)->IsDark = false;
			object = &(*object)->Next;
		}
	}

	freeElapsed = Clock::GetTicks() - freeElapsed;

	Log::Print(Log::LOG_VERBOSE, "Sweep: Graying took %.1f ms", grayElapsed);
	Log::Print(Log::LOG_VERBOSE, "Sweep: Blackening took %.1f ms", blackenElapsed);
	Log::Print(Log::LOG_VERBOSE, "Sweep: Freeing took %.1f ms", freeElapsed);

	for (size_t i = 0; i < MAX_OBJ_TYPE; i++) {
		if (objectTypeFreed[i] && objectTypeCounts[i]) {
			Log::Print(Log::LOG_VERBOSE,
				"Freed %d %s objects out of %d.",
				objectTypeFreed[i],
				GetObjectTypeString(i),
				objectTypeCounts[i]);
		}
	}

	GarbageCollector::NextGC = GarbageCollector::GarbageSize + (1024 * 1024);
}

void GarbageCollector::GrayInstanceables() {
	// Mark model materials
	for (size_t i = 0; i < Material::List.size(); i++) {
		GrayMaterial(Material::List[i]->VMObject);
	}
}

void GarbageCollector::GrayMaterial(void* obj) {
	Obj* object = (Obj*)obj;
	if (!object) {
		return;
	}

	GrayObject(object);

	ObjMaterial* material = (ObjMaterial*)object;
	if (material->MaterialPtr) {
		Material* materialPtr = (Material*)material->MaterialPtr;
		GrayResource(materialPtr->TextureDiffuse);
		GrayResource(materialPtr->TextureSpecular);
		GrayResource(materialPtr->TextureAmbient);
		GrayResource(materialPtr->TextureEmissive);
	}
}

void GarbageCollector::GrayResource(void* ptr) {
	ResourceType* resource = (ResourceType*)ptr;
	if (!resource || !resource->Loaded) {
		return;
	}

	GrayObject(resource->AsAsset->GetVMObjectPtr());

	// Mark model materials
	if (resource->Type == ASSET_MODEL) {
		IModel* model = resource->AsModel;
		for (size_t ii = 0; ii < model->Materials.size(); ii++) {
			GrayMaterial(model->Materials[ii]->VMObject);
		}
	}
}

void GarbageCollector::FreeObject(Obj* object) {
	ScriptManager::DestroyObject(object);
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
	if (object->IsDark) {
		return;
	}

	object->IsDark = true;

	GrayList.push_back(object);
}
void GarbageCollector::GrayHashMapItem(Uint32, VMValue value) {
	GrayValue(value);
}
void GarbageCollector::GrayHashMap(void* pointer) {
	if (!pointer) {
		return;
	}
	((HashMap<VMValue>*)pointer)->ForAll(GrayHashMapItem);
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
		GrayObject(klass->Name);
		GrayHashMap(klass->Methods);
		GrayHashMap(klass->Fields);
		GrayValue(klass->Initializer);
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
		GrayObject(function->Class);
		for (size_t i = 0; i < function->Chunk.Constants->size(); i++) {
			GrayValue((*function->Chunk.Constants)[i]);
		}
		break;
	}
	case OBJ_MODULE: {
		ObjModule* module = (ObjModule*)object;
		GrayObject(module->SourceFilename);
		for (size_t i = 0; i < module->Locals->size(); i++) {
			GrayValue((*module->Locals)[i]);
		}
		for (size_t fn = 0; fn < module->Functions->size(); fn++) {
			GrayObject((*module->Functions)[fn]);
		}
		break;
	}
	case OBJ_INSTANCE:
	case OBJ_ENTITY: {
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
	case OBJ_RESOURCE: {
		ObjResource* resource = (ObjResource*)object;
		GrayResource(resource->ResourcePtr);
		break;
	}
	default:
		// No references
		break;
	}
}

void GarbageCollector::Dispose() {
	Init();
}
