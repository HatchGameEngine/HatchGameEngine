#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ResourceableImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/ImageResourceImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/ResourceTypes/ResourceType.h>

ObjClass* ResourceableImpl::Class = nullptr;

void ResourceableImpl::Init() {
	Class = NewClass(CLASS_RESOURCEABLE);

	TypeImpl::RegisterClass(Class);

	ImageResourceImpl::Init();
}

#define SET_IMPL(className) \
	obj->Object.Class = className::Class; \
	obj->Object.PropertyGet = className::VM_PropertyGet; \
	obj->Object.PropertySet = className::VM_PropertySet

void* ResourceableImpl::New(void* ptr) {
	ObjResourceable* obj =
		(ObjResourceable*)AllocateObject(sizeof(ObjResourceable), OBJ_RESOURCEABLE);
	Memory::Track(obj, "NewResourceable");
	obj->Object.Destructor = ResourceableImpl::Dispose;
	obj->ResourceablePtr = ptr;

	Resourceable* resourceable = (Resourceable*)ptr;
	switch (resourceable->Type) {
	case RESOURCE_IMAGE:
		SET_IMPL(ImageResourceImpl);
		break;
	default:
		break;
	}

	return (void*)obj;
}

#undef SET_IMPL

void ResourceableImpl::Dispose(Obj* object) {
	ObjResourceable* resourceable = (ObjResourceable*)object;

	if (resourceable->ResourceablePtr) {
		((Resourceable*)resourceable->ResourceablePtr)->ReleaseVMObject();
	}
}
