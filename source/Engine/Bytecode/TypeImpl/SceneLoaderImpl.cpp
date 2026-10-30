#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/SceneLoaderImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Scene/SceneLoader.h>

/***
* \class SceneLoader
* \desc A class for reading data from scene files.
*/

ObjClass* SceneLoaderImpl::Class = nullptr;

void SceneLoaderImpl::Init() {
	Class = NewClass("SceneLoader");
	Class->NewFn = Constructor;
	Class->Initializer = OBJECT_VAL(NewNative(VM_Initializer));

#define IMPL_FN(name) \
	ScriptManager::DefineNative(Class, #name, VM_##name);
	SCENELOADERIMPL_FN_LIST
#undef IMPL_FN

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

	// Get a scene loader
	SceneLoader* loader = SceneLoader::New(stream, filename);

	// Read the scene file
	bool success = loader->Read(stream, pathParent);

	// Close the stream. Not needed anymore past this point
	stream->Close();

	// Check if it could be read
	if (!success) {
		char errorString[MAX_RESOURCE_PATH_LENGTH + 30];
		snprintf(errorString, sizeof errorString, "Could not read scene file \"%s\"!", filename);
		throw ScriptException(errorString);
	}

	// Add to registry
	ScriptManager::RegistryAdd(loader, (Obj*)instance);

	// Return the instance
	return OBJECT_VAL(instance);
}
void SceneLoaderImpl::Dispose(Obj* object) {
	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(object);
	if (loader != nullptr) {
		delete loader;
	}
}

#define OUT_OF_RANGE_ERROR(eType, eIdx, eMin, eMax) { \
	char errorString[256]; \
	snprintf(errorString, sizeof errorString, \
		eType " %d out of range. (%d - %d)", \
		eIdx, \
		eMin, \
		eMax); \
	throw ScriptException(errorString); \
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

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	return INTEGER_VAL(loader->TileWidth);
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

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	return INTEGER_VAL(loader->TileHeight);
}

/***
 * \method GetClassCount
 * \desc Gets the amount of classes.
 * \return integer Returns an integer value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetClassCount(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	Obj* obj = AS_OBJECT(args[0]);
	char* objectName = GET_ARG_OPT(1, GetString, nullptr);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	return INTEGER_VAL(loader->GetObjectClassCount());
}
/***
 * \method GetClassName
 * \desc Gets the name of a class.
 * \param classIndex (integer): Index of class.
 * \return string Returns a string value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetClassName(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	if (!loader->IsObjectClassValid(index)) {
		OUT_OF_RANGE_ERROR("Class index", index, 0, loader->GetObjectCount() - 1);
	}

	if (ScriptManager::Lock()) {
		ObjString* string = CopyString(loader->GetObjectClass(index));
		ScriptManager::Unlock();
		return OBJECT_VAL(string);
	}
	return NULL_VAL;
}
/***
 * \method HasClass
 * \desc Checks whether the given class exists.
 * \param className (string): Name of class.
 * \return boolean Returns a boolean value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_HasClass(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	char* className = GET_ARG(1, GetString);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	return INTEGER_VAL(loader->HasObjectClass(className));
}
/***
 * \method LoadClass
 * \desc Loads the given class by name.
 * \param className (string): Name of class.
 * \return boolean Returns whether the class was loaded.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_LoadClass(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	char* className = GET_ARG(1, GetString);

	ObjectList* list = Scene::GetStaticObjectList(className, true);
	if (list != nullptr) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
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

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	int count;
	if (objectName != nullptr) {
		count = loader->GetObjectCount(objectName);
	}
	else {
		count = loader->GetObjectCount();
	}

	return INTEGER_VAL(count);
}
/***
 * \method GetObjectClass
 * \desc Gets the class name of an object.
 * \param objectIndex (integer): Index of object.
 * \return string Returns a string value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetObjectClass(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	SceneLoaderObject* object = loader->GetObject(index);
	if (object == nullptr) {
		OUT_OF_RANGE_ERROR("Object index", index, 0, loader->GetObjectCount() - 1);
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
 * \param objectIndex (integer): Index of object.
 * \return decimal Returns a decimal value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetObjectX(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	SceneLoaderObject* object = loader->GetObject(index);
	if (object == nullptr) {
		OUT_OF_RANGE_ERROR("Object index", index, 0, loader->GetObjectCount() - 1);
	}

	return DECIMAL_VAL(object->X);
}
/***
 * \method GetObjectY
 * \desc Gets the Y position of an object.
 * \param objectIndex (integer): Index of object.
 * \return decimal Returns a decimal value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetObjectY(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	SceneLoaderObject* object = loader->GetObject(index);
	if (object == nullptr) {
		OUT_OF_RANGE_ERROR("Object index", index, 0, loader->GetObjectCount() - 1);
	}

	return DECIMAL_VAL(object->Y);
}
/***
 * \method GetObjectProperty
 * \desc Gets a property from an object.
 * \param objectIndex (integer): Index of object.
 * \param property (string): The property name.
 * \return value Returns the property, if it exists, or `null` if the property does not exist.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetObjectProperty(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);
	char* property = GET_ARG(2, GetString);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	SceneLoaderObject* object = loader->GetObject(index);
	if (object == nullptr) {
		OUT_OF_RANGE_ERROR("Object index", index, 0, loader->GetObjectCount() - 1);
	}

	if (object->Properties && object->Properties->Exists(property)) {
		return Value::FromProperty(object->Properties->Get(property));
	}
	return NULL_VAL;
}
/***
 * \method ObjectHasProperty
 * \desc Checks if a property exists in an object.
 * \param objectIndex (integer): Index of object.
 * \param property (string): The property name.
 * \return boolean Returns a boolean value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_ObjectHasProperty(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);
	char* property = GET_ARG(2, GetString);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	SceneLoaderObject* object = loader->GetObject(index);
	if (object == nullptr) {
		OUT_OF_RANGE_ERROR("Object index", index, 0, loader->GetObjectCount() - 1);
	}

	if (object->Properties && object->Properties->Exists(property)) {
		return INTEGER_VAL(true);
	}
	return INTEGER_VAL(false);
}
/***
 * \method SpawnObject
 * \desc Spawns an object by its index.
 * \param objectIndex (integer): Index of object.
 * \paramOpt x (number): X position of where to place the new entity.
 * \paramOpt y (number): Y position of where to place the new entity.
 * \paramOpt flag (value): Value to pass to the `Create` event. (default: `0`)
 * \paramOpt keepInScene (boolean): Whether to keep the entity in the scene. (default: `true`)
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

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	SceneLoaderObject* object = loader->GetObject(index);
	if (object == nullptr) {
		OUT_OF_RANGE_ERROR("Object index", index, 0, loader->GetObjectCount() - 1);
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

	// Try to load the object class here.
	// If it doesn't exist, fail silently
	if (!Scene::LoadObjectClass(object->Name.c_str())) {
		return NULL_VAL;
	}

	// Attempt to create it
	ScriptEntity* entity = (ScriptEntity*)loader->CreateObject(object, x, y);
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

/***
 * \method GetTilesetCount
 * \desc Gets the amount of tilesets.
 * \return integer Returns an integer value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetTilesetCount(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	Obj* obj = AS_OBJECT(args[0]);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	return INTEGER_VAL(loader->GetTilesetCount());
}
/***
 * \method GetTilesetImagePath
 * \desc Gets the Resource path of the image used by a tileset.
 * \param tilesetIndex (integer): Index of tileset.
 * \return string Returns a string value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetTilesetImagePath(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);
	Tileset* tileset = loader->GetTileset(index);
	if (tileset == nullptr) {
		OUT_OF_RANGE_ERROR("Tileset index", index, 0, loader->GetTilesetCount() - 1);
	}

	if (ScriptManager::Lock()) {
		ObjString* string = CopyString(tileset->Filename);
		ScriptManager::Unlock();
		return OBJECT_VAL(string);
	}
	return NULL_VAL;
}
/***
 * \method AddTileset
 * \desc Adds a tileset to the scene.
 * \param tilesetIndex (integer): Index of tileset.
 * \return integer Returns the index of the added tileset.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_AddTileset(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);
	if (!loader->IsValidTilesetIndex(index)) {
		OUT_OF_RANGE_ERROR("Tileset index", index, 0, loader->GetTilesetCount() - 1);
	}

	int tilesetIndex = loader->AddTileset(index);
	return INTEGER_VAL(tilesetIndex);
}

#define GET_LAYER_PROPERTY(name, field, val) \
VMValue SceneLoaderImpl::VM_GetLayer##name(int argCount, VMValue* args, Uint32 threadID) { \
	StandardLibrary::CheckArgCount(argCount, 2); \
	Obj* obj = AS_OBJECT(args[0]); \
	int index = GET_ARG(1, GetInteger); \
	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj); \
	SceneLayer* layer = loader->GetLayer(index); \
	if (layer == nullptr) { \
		OUT_OF_RANGE_ERROR("Layer index", index, 0, loader->GetLayerCount() - 1); \
	} \
	return val(layer->field); \
}

/***
 * \method GetLayerCount
 * \desc Gets the amount of layers.
 * \return integer Returns an integer value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetLayerCount(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	Obj* obj = AS_OBJECT(args[0]);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	return INTEGER_VAL(loader->GetLayerCount());
}
/***
 * \method GetLayerName
 * \desc Gets the name of a layer.
 * \param layerIndex (integer): Index of layer.
 * \return string Returns a string value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetLayerName(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	SceneLayer* layer = loader->GetLayer(index);
	if (layer == nullptr) {
		OUT_OF_RANGE_ERROR("Layer index", index, 0, loader->GetLayerCount() - 1);
	}

	if (ScriptManager::Lock()) {
		ObjString* string = CopyString(layer->Name);
		ScriptManager::Unlock();
		return OBJECT_VAL(string);
	}
	return NULL_VAL;
}
/***
 * \method GetLayerWidth
 * \desc Gets the width of a layer. For tile layers, returns the width in tiles.
 * \param layerIndex (integer): Index of layer.
 * \return integer Returns an integer value.
 * \ns SceneLoader
 */
GET_LAYER_PROPERTY(Width, Width, INTEGER_VAL)
/***
 * \method GetLayerHeight
 * \desc Gets the height of a layer. For tile layers, returns the height in tiles.
 * \param layerIndex (integer): Index of layer.
 * \return integer Returns an integer value.
 * \ns SceneLoader
 */
GET_LAYER_PROPERTY(Height, Height, INTEGER_VAL)
/***
 * \method GetLayerType
 * \desc Gets the type of the layer.
 * \param layerIndex (integer): Index of layer.
 * \return <ref LAYERTYPE_*> Returns the type of the layer.
 * \ns SceneLoader
 */
GET_LAYER_PROPERTY(Type, Type, INTEGER_VAL)
/***
 * \method GetLayerDrawGroup
 * \desc Gets the draw group of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \return integer Returns an integer value.
 * \ns SceneLoader
 */
GET_LAYER_PROPERTY(DrawGroup, DrawGroup, INTEGER_VAL)

/***
 * \method GetLayerOffsetX
 * \desc Gets the X offset of a layer.
 * \param layerIndex (integer): Index of layer.
 * \return decimal Returns a decimal value.
 * \ns SceneLoader
 */
GET_LAYER_PROPERTY(OffsetX, OffsetX, DECIMAL_VAL)
/***
 * \method GetLayerOffsetY
 * \desc Gets the Y offset of a layer.
 * \param layerIndex (integer): Index of layer.
 * \return decimal Returns a decimal value.
 * \ns SceneLoader
 */
GET_LAYER_PROPERTY(OffsetY, OffsetY, DECIMAL_VAL)

/***
 * \method GetLayerHorizontalParallaxFactor
 * \desc Gets the horizontal parallax scroll factor of the layer.
 * \param layerIndex (integer): Index of layer.
 * \return decimal Returns a decimal value.
 * \ns SceneLoader
 */
GET_LAYER_PROPERTY(HorizontalParallaxFactor, RelativeX, DECIMAL_VAL)
/***
 * \method GetLayerVerticalParallaxFactor
 * \desc Gets the vertical parallax scroll factor of the layer.
 * \param layerIndex (integer): Index of layer.
 * \return decimal Returns a decimal value.
 * \ns SceneLoader
 */
GET_LAYER_PROPERTY(VerticalParallaxFactor, RelativeY, DECIMAL_VAL)

/***
 * \method GetLayerHorizontalConstantScroll
 * \desc Gets the horizontal constant scroll amount of the layer.
 * \param layerIndex (integer): Index of layer.
 * \return decimal Returns a decimal value.
 * \ns SceneLoader
 */
GET_LAYER_PROPERTY(HorizontalConstantScroll, ConstantX, DECIMAL_VAL)
/***
 * \method GetLayerVerticalConstantScroll
 * \desc Gets the vertical constant scroll amount of the layer.
 * \param layerIndex (integer): Index of layer.
 * \return decimal Returns a decimal value.
 * \ns SceneLoader
 */
GET_LAYER_PROPERTY(VerticalConstantScroll, ConstantY, DECIMAL_VAL)

/***
 * \method GetLayerProperty
 * \desc Gets a property from a layer.
 * \param layerIndex (integer): Index of layer.
 * \param property (string): The property name.
 * \return value Returns the property, if it exists, or `null` if the property does not exist.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_GetLayerProperty(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);
	char* property = GET_ARG(2, GetString);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	SceneLayer* layer = loader->GetLayer(index);
	if (layer == nullptr) {
		OUT_OF_RANGE_ERROR("Layer index", index, 0, loader->GetLayerCount() - 1);
	}

	if (layer->Properties && layer->Properties->Exists(property)) {
		return Value::FromProperty(layer->Properties->Get(property));
	}
	return NULL_VAL;
}
/***
 * \method LayerHasProperty
 * \desc Checks if a property exists in a layer.
 * \param layerIndex (integer): Index of layer.
 * \param property (string): The property name.
 * \return boolean Returns a boolean value.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_LayerHasProperty(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);
	char* property = GET_ARG(2, GetString);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	SceneLayer* layer = loader->GetLayer(index);
	if (layer == nullptr) {
		OUT_OF_RANGE_ERROR("Layer index", index, 0, loader->GetLayerCount() - 1);
	}

	if (layer->Properties && layer->Properties->Exists(property)) {
		return INTEGER_VAL(true);
	}
	return INTEGER_VAL(false);
}
/***
 * \method AddLayer
 * \desc Adds a layer to the scene.
 * \param layerIndex (integer): Index of layer.
 * \paramOpt keepInScene (boolean): Whether to keep the layer in the scene. (default: `true`)
 * \paramOpt offsetX (integer): X offset of the layer. If adding a tile layer, the given offset is in tiles.
 * \paramOpt offsetY (integer): Y offset of the layer. If adding a tile layer, the given offset is in tiles.
 * \return integer Returns the index of the added layer.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_AddLayer(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 2);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);
	bool keepInScene = GET_ARG_OPT(2, GetInteger, true);
	int offsetX = GET_ARG_OPT(3, GetInteger, 0);
	int offsetY = GET_ARG_OPT(4, GetInteger, 0);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	SceneLayer* layer = loader->GetLayer(index);
	if (layer == nullptr) {
		OUT_OF_RANGE_ERROR("Layer index", index, 0, loader->GetLayerCount() - 1);
	}

	int layerIndex = loader->AddLayer(layer, keepInScene, false, offsetX, offsetY);
	return INTEGER_VAL(layerIndex);
}
/***
 * \method MergeLayer
 * \desc Adds a layer to the scene, by merging it with another layer in the current scene.
 * \param layerIndex (integer): Index of layer.
 * \param destLayerIndex (integer): Index of the destination layer in the current scene.
 * \paramOpt offsetX (integer): X offset of the layer. If it'sa tile layer, the given offset is in tiles.
 * \paramOpt offsetY (integer): Y offset of the layer. If it's a tile layer, the given offset is in tiles.
 * \return boolean Returns whether the layer could be merged.
 * \ns SceneLoader
 */
VMValue SceneLoaderImpl::VM_MergeLayer(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 3);

	Obj* obj = AS_OBJECT(args[0]);
	int index = GET_ARG(1, GetInteger);
	int destLayerIndex = GET_ARG(2, GetInteger);
	int offsetX = GET_ARG_OPT(3, GetInteger, 0);
	int offsetY = GET_ARG_OPT(4, GetInteger, 0);

	SceneLoader* loader = (SceneLoader*)ScriptManager::RegistryGet(obj);

	SceneLayer* layer = loader->GetLayer(index);
	if (layer == nullptr) {
		OUT_OF_RANGE_ERROR("Loaded layer index", index, 0, loader->GetLayerCount() - 1);
	}

	if (destLayerIndex < 0 || destLayerIndex >= (int)Scene::Layers.size()) {
		OUT_OF_RANGE_ERROR("Destination layer index", destLayerIndex, 0, (int)Scene::Layers.size() - 1);
	}
	if (Scene::Layers[destLayerIndex] == nullptr) {
		char errorString[64];
		snprintf(errorString, sizeof errorString, "Destination layer index %d is invalid.", destLayerIndex);
		throw ScriptException(errorString);
	}

	if (loader->AddLayerIfCanMerge(layer, Scene::Layers[destLayerIndex], offsetX, offsetY)) {
		return INTEGER_VAL(true);
	}
	return INTEGER_VAL(false);
}

#undef GET_LAYER_INTEGER_PROPERTY

#undef GET_ARG
#undef GET_ARG_OPT
#undef OUT_OF_RANGE_ERROR
