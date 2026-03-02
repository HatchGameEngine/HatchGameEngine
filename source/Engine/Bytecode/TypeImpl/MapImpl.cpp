#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/MapImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

MapImpl::MapImpl(ScriptManager* manager) {
	Manager = manager;
	Class = Manager->NewClass(CLASS_MAP);

#ifdef HSL_VM
	Manager->DefineNative(Class, "keys", VM_GetKeys);
	Manager->DefineNative(Class, "remove", VM_RemoveKey);
	Manager->DefineNative(Class, "iterate", VM_Iterate);
	Manager->DefineNative(Class, "iteratorValue", VM_IteratorValue);
#endif

	TypeImpl::RegisterClass(manager, Class);
}

Obj* MapImpl::New() {
	ObjMap* map = (ObjMap*)Manager->AllocateObject(sizeof(ObjMap), OBJ_MAP);
	Memory::Track(map, "NewMap");
	map->Object.Class = Class;
	map->Values = new OrderedHashMap<VMValue>(NULL, 4);
	map->Keys = new OrderedHashMap<char*>(NULL, 4);
	return (Obj*)map;
}

void MapImpl::Dispose(Obj* object) {
	ObjMap* map = (ObjMap*)object;

	// Free keys
	map->Keys->WithAll([](Uint32, char* ptr) -> void {
		Memory::Free(ptr);
	});

	// Free Keys table
	delete map->Keys;

	// Free Values table
	delete map->Values;
}

#ifdef HSL_VM
#define GET_ARG(argIndex, argFunction) (thread->Manager->argFunction(args, argIndex, thread))

VMValue MapImpl::VM_GetKeys(int argCount, VMValue* args, VMThread* thread) {
	ScriptManager::CheckArgCount(argCount, 1, thread);

	ObjMap* map = GET_ARG(0, GetMap);

	ObjArray* array = thread->Manager->NewArray();

	map->Keys->WithAllOrdered([thread, array](Uint32, char* key) -> void {
		array->Values->push_back(OBJECT_VAL(thread->Manager->CopyString(key)));
	});

	return OBJECT_VAL(array);
}

VMValue MapImpl::VM_RemoveKey(int argCount, VMValue* args, VMThread* thread) {
	ScriptManager::CheckArgCount(argCount, 2, thread);

	ObjMap* map = GET_ARG(0, GetMap);
	const char* key = GET_ARG(1, GetString);

	map->Keys->Remove(key);
	map->Values->Remove(key);

	return NULL_VAL;
}

VMValue MapImpl::VM_Iterate(int argCount, VMValue* args, VMThread* thread) {
	ScriptManager::CheckArgCount(argCount, 2, thread);

	ObjMap* map = GET_ARG(0, GetMap);

	int key;
	if (IS_NULL(args[1])) {
		key = map->Values->GetFirstKey();
	}
	else {
		key = map->Values->GetNextKey(GET_ARG(1, GetInteger));
	}

	if (key) {
		return INTEGER_VAL(key);
	}

	return NULL_VAL;
}

VMValue MapImpl::VM_IteratorValue(int argCount, VMValue* args, VMThread* thread) {
	ScriptManager::CheckArgCount(argCount, 2, thread);

	ObjMap* map = GET_ARG(0, GetMap);
	int key = GET_ARG(1, GetInteger);

	VMValue value;
	if (map->Values->GetIfExists(key, &value)) {
		return value;
	}

	return NULL_VAL;
}
#endif
