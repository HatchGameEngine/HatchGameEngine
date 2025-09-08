#include <Engine/ResourceTypes/Resourceable.h>
#include <Engine/Bytecode/TypeImpl/ResourceableImpl.h>

bool Resourceable::IsLoaded() {
	return Loaded;
}

void Resourceable::TakeRef() {
	RefCount++;
}

bool Resourceable::Release() {
	if (RefCount == 0) {
		Error::Fatal("Tried to release reference of Resourceable when it had none!");
	}

	RefCount--;

	if (RefCount == 0) {
		delete this;

		return true;
	}

	return false;
}

void Resourceable::Unload() {}

void* Resourceable::GetVMObject() {
	if (VMObject == nullptr) {
		VMObject = ResourceableImpl::New(this);
	}

	return VMObject;
}

void* Resourceable::GetVMObjectPtr() {
	return VMObject;
}

void Resourceable::ReleaseVMObject() {
	if (VMObject != nullptr) {
		((ObjResourceable*)VMObject)->ResourceablePtr = nullptr;
		VMObject = nullptr;
	}
}

Resourceable::~Resourceable() {
	ReleaseVMObject();
}
