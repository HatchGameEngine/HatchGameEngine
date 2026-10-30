#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/SceneLoaderImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Scene/SceneReader.h>

/***
* \class SceneLoader
* \desc A class for reading data from scene files.
*/

ObjClass* SceneLoaderImpl::Class = nullptr;

void SceneLoaderImpl::Init() {
	Class = NewClass(CLASS_SCENELOADER);
	Class->NewFn = Constructor;
	Class->Initializer = OBJECT_VAL(NewNative(VM_Initializer));

	ScriptManager::DefineNative(Class, "GetTileWidth", VM_GetTileWidth);
	ScriptManager::DefineNative(Class, "GetTileHeight", VM_GetTileHeight);
	ScriptManager::DefineNative(Class, "GetObjectCount", VM_GetObjectCount);
	ScriptManager::DefineNative(Class, "GetObjectClass", VM_GetObjectClass);
	ScriptManager::DefineNative(Class, "GetObjectX", VM_GetObjectX);
	ScriptManager::DefineNative(Class, "GetObjectY", VM_GetObjectY);
	ScriptManager::DefineNative(Class, "GetObjectProperty", VM_GetObjectProperty);
	ScriptManager::DefineNative(Class, "ObjectHasProperty", VM_ObjectHasProperty);
	ScriptManager::DefineNative(Class, "SpawnObject", VM_SpawnObject);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(Class);
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, StandardLibrary::argFunction) : argDefault)

Obj* SceneLoaderImpl::Constructor() {
	ObjInstance* instance = (ObjInstance*)NewNativeInstance(sizeof(ObjInstance));
	Memory::Track(instance, "NewSceneLoader");
	instance->Object.Class = Class;
	instance->Destructor = Dispose;
	return (Obj*)instance;
}

/***
 * \constructor
 * \desc Loads a scene from a Resource.
 * \param filename (string): Filename of the resource.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_Initializer(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjInstance* instance = AS_INSTANCE(args[0]);
	char* filename = GET_ARG(1, GetString);

	// Attempt to open the stream
	Stream* stream = ResourceStream::New(filename);
	if (!stream) {
		char errorString[MAX_RESOURCE_PATH_LENGTH + 35];
		snprintf(errorString, sizeof errorString, "Could not open resource stream \"%s\"!", filename);
		throw ScriptException(errorString);
	}

	// Get the parent path
	char pathParent[MAX_RESOURCE_PATH_LENGTH];
	const char* sep = strrchr(filename, '/');
	if (sep != nullptr) {
		size_t length = (sep - filename) + 1;
		memcpy(pathParent, filename, length);
		pathParent[length] = '\0';
	}
	else {
		pathParent[0] = '\0';
	}

	// Get a scene reader
	SceneReader* reader = SceneReader::New(stream, filename);

	// Read the scene file
	bool success = reader->Read(stream, pathParent);

	// Close the stream. Not needed anymore past this point
	stream->Close();

	// Check if it could be read
	if (!success) {
		char errorString[MAX_RESOURCE_PATH_LENGTH + 30];
		snprintf(errorString, sizeof errorString, "Could not read scene file \"%s\"!", filename);
		throw ScriptException(errorString);
	}

	// Add to registry
	ScriptManager::RegistryAdd(reader, (Obj*)instance);

	// Return the instance
	return OBJECT_VAL(instance);
}
void SceneLoaderImpl::Dispose(Obj* object) {
	SceneReader* reader = (SceneReader*)ScriptManager::RegistryGet(object);
	if (reader != nullptr) {
		delete reader;
	}
}

/***
 * \method GetTileWidth
 * \desc Gets the width of tiles.
 * \return integer Returns an integer value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetTileWidth(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	Obj* obj = AS_OBJECT(args[0]);

	SceneReader* reader = (SceneReader*)ScriptManager::RegistryGet(obj);

	return INTEGER_VAL(reader->TileWidth);
}
/***
 * \method GetTileHeight
 * \desc Gets the height of tiles.
 * \return integer Returns an integer value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetTileHeight(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	Obj* obj = AS_OBJECT(args[0]);

	SceneReader* reader = (SceneReader*)ScriptManager::RegistryGet(obj);

	return INTEGER_VAL(reader->TileHeight);
}
/***
 * \method GetObjectCount
 * \desc Gets the amount of objects.
 * \paramOpt className (string): Name of the object class to count.
 * \return integer Returns an integer value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetObjectCount(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 1);

	Obj* obj = AS_OBJECT(args[0]);
	char* objectName = GET_ARG_OPT(1, GetString, nullptr);

	SceneReader* reader = (SceneReader*)ScriptManager::RegistryGet(obj);

	int count;
	if (objectName != nullptr) {
		count = reader->GetObjectCount(objectName);
	}
	else {
		count = reader->GetObjectCount();
	}

	return INTEGER_VAL(count);
}
/***
 * \method GetObjectClass
 * \desc Gets the class name of an object.
 * \param index (integer): Index of the object to check.
 * \return string Returns a string value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetObjectClass(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);

	SceneReader* reader = (SceneReader*)ScriptManager::RegistryGet(obj);

	SceneReaderObject* object = reader->GetObject(index);
	if (object == nullptr) {
		ScriptManager::Threads[0].ThrowRuntimeError(false,
			"Object %d out of range. (%d - %d)",
			index,
			0,
			reader->GetObjectCount() - 1);
		return NULL_VAL;
	}

	if (ScriptManager::Lock()) {
		ObjString* string = CopyString(object->Name);
		ScriptManager::Unlock();
		return OBJECT_VAL(string);
	}
	return NULL_VAL;
}
/***
 * \method GetObjectX
 * \desc Gets the X position of an object.
 * \param index (integer): Index of the object to check.
 * \return decimal Returns a decimal value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetObjectX(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);

	SceneReader* reader = (SceneReader*)ScriptManager::RegistryGet(obj);

	SceneReaderObject* object = reader->GetObject(index);
	if (object == nullptr) {
		ScriptManager::Threads[0].ThrowRuntimeError(false,
			"Object %d out of range. (%d - %d)",
			index,
			0,
			reader->GetObjectCount() - 1);
		return NULL_VAL;
	}

	return DECIMAL_VAL(object->X);
}
/***
 * \method GetObjectY
 * \desc Gets the Y position of an object.
 * \param index (integer): Index of the object to check.
 * \return decimal Returns a decimal value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetObjectY(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);

	SceneReader* reader = (SceneReader*)ScriptManager::RegistryGet(obj);

	SceneReaderObject* object = reader->GetObject(index);
	if (object == nullptr) {
		ScriptManager::Threads[0].ThrowRuntimeError(false,
			"Object %d out of range. (%d - %d)",
			index,
			0,
			reader->GetObjectCount() - 1);
		return NULL_VAL;
	}

	return DECIMAL_VAL(object->Y);
}
/***
 * \method GetObjectProperty
 * \desc Gets a property from an object.
 * \param index (integer): Index of the object to check.
 * \param property (string): The property name.
 * \return value Returns the property, if it exists, or `null` if the property does not exist.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetObjectProperty(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);
	char* property = GET_ARG(2, GetString);

	SceneReader* reader = (SceneReader*)ScriptManager::RegistryGet(obj);

	SceneReaderObject* object = reader->GetObject(index);
	if (object == nullptr) {
		ScriptManager::Threads[0].ThrowRuntimeError(false,
			"Object %d out of range. (%d - %d)",
			index,
			0,
			reader->GetObjectCount() - 1);
		return NULL_VAL;
	}

	if (object->Properties && object->Properties->Exists(property)) {
		return Value::FromProperty(object->Properties->Get(property));
	}
	return NULL_VAL;
}
/***
 * \method ObjectHasProperty
 * \desc Checks if a property exists in an object.
 * \param index (integer): Index of the object to check.
 * \param property (string): The property name.
 * \return boolean Returns a boolean value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_ObjectHasProperty(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);
	char* property = GET_ARG(2, GetString);

	SceneReader* reader = (SceneReader*)ScriptManager::RegistryGet(obj);

	SceneReaderObject* object = reader->GetObject(index);
	if (object == nullptr) {
		ScriptManager::Threads[0].ThrowRuntimeError(false,
			"Object %d out of range. (%d - %d)",
			index,
			0,
			reader->GetObjectCount() - 1);
		return NULL_VAL;
	}

	if (object->Properties && object->Properties->Exists(property)) {
		return INTEGER_VAL(true);
	}
	return INTEGER_VAL(false);
}
/***
 * \method SpawnObject
 * \desc Spawns an object by its index.
 * \param index (integer): Index of the object to spawn.
 * \paramOpt x (number): X position of where to place the new entity.
 * \paramOpt y (number): Y position of where to place the new entity.
 * \paramOpt flag (value): Value to pass to the `Create` event. (default: `0`)
 * \paramOpt keepInScene (boolean): Whether to keep the entity in the scene.
 * \return integer Returns an integer value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_SpawnObject(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);
	float x;
	float y;
	VMValue flag = argCount == 5 ? args[4] : INTEGER_VAL(0);
	bool keepInScene = GET_ARG_OPT(5, GetInteger, true);

	SceneReader* reader = (SceneReader*)ScriptManager::RegistryGet(obj);

	SceneReaderObject* object = reader->GetObject(index);
	if (object == nullptr) {
		ScriptManager::Threads[0].ThrowRuntimeError(false,
			"Object %d out of range. (%d - %d)",
			index,
			0,
			reader->GetObjectCount() - 1);
		return NULL_VAL;
	}

	if (argCount >= 3 && !IS_NULL(args[2])) {
		x = GET_ARG(2, GetDecimal);
	}
	else {
		x = object->X;
	}
	if (argCount >= 4 && !IS_NULL(args[3])) {
		y = GET_ARG(3, GetDecimal);
	}
	else {
		y = object->Y;
	}

	ScriptEntity* entity = (ScriptEntity*)reader->CreateObject(object, x, y);
	if (entity == nullptr) {
		return NULL_VAL;
	}

	// Call the initializer
	entity->Initialize();

	// Add it to the scene
	if (keepInScene) {
		Scene::AddStatic(entity->List, entity);
	}
	else {
		Scene::AddDynamic(entity->List, entity);

		// Add to proper list
		if (entity->List) {
			entity->List->Add(entity);
		}
	}

	entity->Create(flag);
	if (!Scene::Initializing) {
		entity->PostCreate();
	}

	return OBJECT_VAL(entity->Instance);
}

#undef GET_ARG
#undef GET_ARG_OPT
