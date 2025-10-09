#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/AssetImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/AudioImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/ImageImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/SpriteImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/ResourceTypes/ResourceType.h>

ObjClass* AssetImpl::Class = nullptr;

#define CLASS_ASSET "Asset"

void AssetImpl::Init() {
	Class = NewClass(CLASS_ASSET);

	TypeImpl::RegisterClass(Class);

	AudioImpl::Init();
	ImageImpl::Init();
	SpriteImpl::Init();
}

void* AssetImpl::New(void* ptr) {
	ObjAsset* obj = (ObjAsset*)AllocateObject(sizeof(ObjAsset), OBJ_ASSET);
	Memory::Track(obj, "NewAsset");
	obj->Object.Destructor = AssetImpl::Dispose;
	obj->AssetPtr = ptr;

#define CASE(type, className) \
	case ASSET_##type: \
		obj->Object.Class = className##Impl::Class; \
		obj->Object.PropertyGet = className##Impl::VM_PropertyGet; \
		obj->Object.PropertySet = className##Impl::VM_PropertySet; \
		break

	Asset* asset = (Asset*)ptr;
	switch (asset->Type) {
	CASE(AUDIO, Audio);
	CASE(IMAGE, Image);
	CASE(SPRITE, Sprite);
	default:
		break;
	}

#undef CASE

	return (void*)obj;
}

ValueGetFn AssetImpl::GetGetter(AssetType type) {
#define CASE(type, className) \
	case ASSET_##type: \
		return className##Impl::VM_PropertyGet \

	switch (type) {
	CASE(AUDIO, Audio);
	CASE(IMAGE, Image);
	CASE(SPRITE, Sprite);
	default:
		return nullptr;
	}

#undef CASE
}
ValueSetFn AssetImpl::GetSetter(AssetType type) {
#define CASE(type, className) \
	case ASSET_##type: \
		return className##Impl::VM_PropertySet \

	switch (type) {
	CASE(AUDIO, Audio);
	CASE(IMAGE, Image);
	CASE(SPRITE, Sprite);
	default:
		return nullptr;
	}

#undef CASE
}

void AssetImpl::Dispose(Obj* object) {
	ObjAsset* asset = (ObjAsset*)object;

	if (asset->AssetPtr) {
		((Asset*)asset->AssetPtr)->ReleaseVMObject();
	}
}
