#include <Engine/ResourceTypes/Resourceable.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl.h>

bool Resourceable::Loaded() {
	return !LoadFailed;
}

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
