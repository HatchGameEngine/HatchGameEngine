#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/MapImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* MapImpl::Class = nullptr;

void MapImpl::Init() {
	Class = NewClass(CLASS_MAP);

	ScriptManager::DefineNative(Class, "keys", MapImpl::VM_GetKeys);
	ScriptManager::DefineNative(Class, "remove", MapImpl::VM_RemoveKey);
	ScriptManager::DefineNative(Class, "iterate", MapImpl::VM_Iterate);
	ScriptManager::DefineNative(Class, "iteratorValue", MapImpl::VM_IteratorValue);

	TypeImpl::RegisterClass(Class);
}

Obj* MapImpl::New() {
	ObjMap* map = (ObjMap*)AllocateObject(sizeof(ObjMap), OBJ_MAP);
	Memory::Track(map, "NewMap");
	map->Object.Class = Class;
	map->Object.Destructor = Dispose;
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

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

VMValue MapImpl::VM_GetKeys(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjMap* map = GET_ARG(0, GetMap);

	ObjArray* array = NewArray();

	map->Keys->WithAllOrdered([array](Uint32, char* key) -> void {
		array->Values->push_back(OBJECT_VAL(CopyString(key)));
	});

	return OBJECT_VAL(array);
}

VMValue MapImpl::VM_RemoveKey(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjMap* map = GET_ARG(0, GetMap);
	const char* key = GET_ARG(1, GetString);

	map->Keys->Remove(key);
	map->Values->Remove(key);

	return NULL_VAL;
}

VMValue MapImpl::VM_Iterate(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

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

VMValue MapImpl::VM_IteratorValue(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjMap* map = GET_ARG(0, GetMap);
	int key = GET_ARG(1, GetInteger);

	VMValue value;
	if (map->Values->GetIfExists(key, &value)) {
		return value;
	}

	return NULL_VAL;
}
