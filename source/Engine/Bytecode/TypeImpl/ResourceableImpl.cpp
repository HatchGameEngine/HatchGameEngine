#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ResourceableImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/AudioImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/ImageImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/SpriteImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/ResourceTypes/ResourceType.h>

ObjClass* ResourceableImpl::Class = nullptr;

void ResourceableImpl::Init() {
	Class = NewClass(CLASS_RESOURCEABLE);

	TypeImpl::RegisterClass(Class);

	AudioImpl::Init();
	ImageImpl::Init();
	SpriteImpl::Init();
}

void* ResourceableImpl::New(void* ptr) {
	ObjResourceable* obj =
		(ObjResourceable*)AllocateObject(sizeof(ObjResourceable), OBJ_RESOURCEABLE);
	Memory::Track(obj, "NewResourceable");
	obj->Object.Destructor = ResourceableImpl::Dispose;
	obj->ResourceablePtr = ptr;

#define CASE(type, className) \
	case RESOURCE_##type: \
		obj->Object.Class = className##Impl::Class; \
		obj->Object.PropertyGet = className##Impl::VM_PropertyGet; \
		obj->Object.PropertySet = className##Impl::VM_PropertySet; \
		break

	Resourceable* resourceable = (Resourceable*)ptr;
	switch (resourceable->Type) {
	CASE(AUDIO, Audio);
	CASE(IMAGE, Image);
	CASE(SPRITE, Sprite);
	default:
		break;
	}

#undef CASE

	return (void*)obj;
}

ValueGetFn ResourceableImpl::GetGetter(Uint8 type) {
#define CASE(type, className) \
	case RESOURCE_##type: \
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
ValueSetFn ResourceableImpl::GetSetter(Uint8 type) {
#define CASE(type, className) \
	case RESOURCE_##type: \
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

void ResourceableImpl::Dispose(Obj* object) {
	ObjResourceable* resourceable = (ObjResourceable*)object;

	if (resourceable->ResourceablePtr) {
		((Resourceable*)resourceable->ResourceablePtr)->ReleaseVMObject();
	}
}
