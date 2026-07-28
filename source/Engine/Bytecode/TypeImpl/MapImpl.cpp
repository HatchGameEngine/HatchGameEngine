#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/MapImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>

/***
* \class Map
* \desc An associative array, also known as a dictionary, or a map.
*/

ObjClass* MapImpl::Class = nullptr;

void MapImpl::Init() {
	Class = NewClass("Map");
	Class->NewFn = Constructor;

	ScriptManager::DefineNative(Class, "Length", MapImpl::VM_Length);
	ScriptManager::DefineNative(Class, "GetKeys", MapImpl::VM_GetKeys);
	ScriptManager::DefineNative(Class, "keys", MapImpl::VM_keys);
	ScriptManager::DefineNative(Class, "Remove", MapImpl::VM_Remove);
	ScriptManager::DefineNative(Class, "remove", MapImpl::VM_Remove);
	ScriptManager::DefineNative(Class, "iterate", MapImpl::VM_Iterate);
	ScriptManager::DefineNative(Class, "iteratorValue", MapImpl::VM_IteratorValue);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(Class);
}

/***
 * \constructor
 * \desc Creates a map.
 * \ns Map
 */
Obj* MapImpl::Constructor() {
	ObjMap* map = (ObjMap*)AllocateObject(sizeof(ObjMap), OBJ_MAP);
	Memory::Track(map, "NewMap");
	map->Object.Class = Class;
	map->Values = new OrderedHashMap<VMValue>(NULL, 4);
	map->Keys = new OrderedHashMap<VMValue>(NULL, 4);
	return (Obj*)map;
}

void MapImpl::Dispose(Obj* object) {
	ObjMap* map = (ObjMap*)object;

	// Free Keys table
	delete map->Keys;

	// Free Values table
	delete map->Values;
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

/***
 * \method Length
 * \desc Get the number of items in the map.
 * \return integer Returns an integer value.
 * \ns Map
 */
VMValue MapImpl::VM_Length(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjMap* map = GET_ARG(0, GetMap);

	return INTEGER_VAL((int)map->Keys->Count());
}

/***
 * \method GetKeys
 * \desc Gets a list of all keys in the map. This always returns the same array reference.
 * \return array Returns an array of values.
 * \ns Map
 */
VMValue MapImpl::VM_GetKeys(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjMap* map = GET_ARG(0, GetMap);
	ObjArray* array = map->KeysArray;

	if (array == nullptr) {
		array = (ObjArray*)NewArray();
		map->KeysArray = array;
	}
	else {
		array->Values->clear();
	}

	map->Keys->WithAllOrdered([array](Uint32, VMValue value) -> void {
		array->Values->push_back(value);
	});

	return OBJECT_VAL(array);
}

VMValue MapImpl::VM_keys(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjMap* map = GET_ARG(0, GetMap);

	ObjArray* array = NewArray();

	map->Keys->WithAllOrdered([array](Uint32, VMValue value) -> void {
		array->Values->push_back(value);
	});

	return OBJECT_VAL(array);
}

/***
 * \method Remove
 * \desc Removes a key from the map.
 * \param key (value): The key to remove.
 * \ns Map
 */
VMValue MapImpl::VM_Remove(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjMap* map = GET_ARG(0, GetMap);

	map->Remove(args[1]);

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

	if (key != 0xFFFFFFFF) {
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
