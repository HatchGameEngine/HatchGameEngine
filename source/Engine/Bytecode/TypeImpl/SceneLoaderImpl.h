#ifndef ENGINE_BYTECODE_TYPEIMPL_SCENELOADERIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_SCENELOADERIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define SCENELOADERIMPL_FN_LIST \
	IMPL_FN(GetTileWidth) \
	IMPL_FN(GetTileHeight) \
	IMPL_FN(GetClassCount) \
	IMPL_FN(GetClassName) \
	IMPL_FN(HasClass) \
	IMPL_FN(LoadClass) \
	IMPL_FN(GetObjectCount) \
	IMPL_FN(GetObjectClass) \
	IMPL_FN(GetObjectX) \
	IMPL_FN(GetObjectY) \
	IMPL_FN(GetObjectProperty) \
	IMPL_FN(ObjectHasProperty) \
	IMPL_FN(SetObjectClass) \
	IMPL_FN(SetObjectX) \
	IMPL_FN(SetObjectY) \
	IMPL_FN(SetObjectProperty) \
	IMPL_FN(RemoveObjectProperty) \
	IMPL_FN(SpawnObject) \
	IMPL_FN(GetTilesetCount) \
	IMPL_FN(GetTilesetImagePath) \
	IMPL_FN(AddTileset) \
	IMPL_FN(GetLayerCount) \
	IMPL_FN(GetLayerName) \
	IMPL_FN(GetLayerWidth) \
	IMPL_FN(GetLayerHeight) \
	IMPL_FN(GetLayerType) \
	IMPL_FN(GetLayerDrawGroup) \
	IMPL_FN(GetLayerOffsetX) \
	IMPL_FN(GetLayerOffsetY) \
	IMPL_FN(GetLayerHorizontalParallaxFactor) \
	IMPL_FN(GetLayerVerticalParallaxFactor) \
	IMPL_FN(GetLayerHorizontalConstantScroll) \
	IMPL_FN(GetLayerVerticalConstantScroll) \
	IMPL_FN(GetLayerProperty) \
	IMPL_FN(LayerHasProperty) \
	IMPL_FN(AddLayer) \
	IMPL_FN(MergeLayer)

class SceneLoaderImpl {
public:
	static ObjClass* Class;

	static void Init();

	static Obj* Constructor();
	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	static void Dispose(Obj* object);

#define IMPL_FN(name) \
	static VMValue VM_##name(int argCount, VMValue* args, Uint32 threadID);
	SCENELOADERIMPL_FN_LIST
#undef IMPL_FN
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_SCENELOADERIMPL_H */
