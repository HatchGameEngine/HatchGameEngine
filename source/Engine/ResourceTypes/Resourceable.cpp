#include <Engine/ResourceTypes/Resourceable.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl.h>

bool Resourceable::IsLoaded() {
	return Loaded;
}

void Resourceable::TakeRef() {
	RefCount++;
}

bool Resourceable::Release() {
	if (RefCount == 0) {
		abort();
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
		VMObject = ResourceImpl::NewResourceableObject(this);
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
