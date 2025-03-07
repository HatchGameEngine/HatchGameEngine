#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/MapImpl.h>

ObjClass* MapImpl::Class = nullptr;

void MapImpl::Init() {
	const char* name = "$$MapImpl";

	Class = NewClass(Murmur::EncryptString(name));
	Class->Name = CopyString(name);

	ScriptManager::DefineNative(Class, "keys", MapImpl::VM_GetKeys);
	ScriptManager::DefineNative(Class, "iterate", MapImpl::VM_Iterate);
	ScriptManager::DefineNative(Class, "iteratorValue", MapImpl::VM_IteratorValue);

	ScriptManager::ClassImplList.push_back(Class);
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
