#ifndef ENGINE_BYTECODE_TYPEIMPL_SCENELOADERIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_SCENELOADERIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define CLASS_SCENELOADER "SceneLoader"

class SceneLoaderImpl {
public:
	static ObjClass* Class;

	static void Init();

	static Obj* Constructor();
	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	static void Dispose(Obj* object);

	static VMValue VM_GetTileWidth(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetTileHeight(int argCount, VMValue* args, Uint32 threadID);

	static VMValue VM_GetClassCount(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetClassName(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_HasClass(int argCount, VMValue* args, Uint32 threadID);

	static VMValue VM_GetObjectCount(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetObjectClass(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetObjectX(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetObjectY(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetObjectProperty(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_ObjectHasProperty(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SpawnObject(int argCount, VMValue* args, Uint32 threadID);

	static VMValue VM_GetTilesetCount(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetTilesetImagePath(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_AddTileset(int argCount, VMValue* args, Uint32 threadID);

	static VMValue VM_GetLayerCount(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerName(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerWidth(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerHeight(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerType(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerDrawGroup(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerOffsetX(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerOffsetY(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerHorizontalParallaxFactor(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerVerticalParallaxFactor(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerHorizontalConstantScroll(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerVerticalConstantScroll(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetLayerProperty(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_LayerHasProperty(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_AddLayer(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_MergeLayer(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_SCENELOADERIMPL_H */
