#include <Engine/Error.h>
#include <Engine/ResourceTypes/Asset.h>
#include <Engine/Bytecode/TypeImpl/AssetImpl.h>

bool Asset::IsLoaded() {
	return Loaded;
}

void Asset::TakeRef() {
	RefCount++;
}

bool Asset::Release() {
	if (RefCount == 0) {
		Error::Fatal("Tried to release reference of Asset when it had none!");
	}

	RefCount--;

	if (RefCount == 0) {
		delete this;

		return true;
	}

	return false;
}

void Asset::Unload() {}

void* Asset::GetVMObject() {
	if (VMObject == nullptr) {
		VMObject = AssetImpl::New(this);
	}

	return VMObject;
}

void* Asset::GetVMObjectPtr() {
	return VMObject;
}

void Asset::ReleaseVMObject() {
	if (VMObject != nullptr) {
		((ObjAsset*)VMObject)->AssetPtr = nullptr;
		VMObject = nullptr;
	}
}

Asset::~Asset() {
	ReleaseVMObject();
}
