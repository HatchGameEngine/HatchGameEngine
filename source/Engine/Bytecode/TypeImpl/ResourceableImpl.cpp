#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ResourceableImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/ResourceTypes/ResourceType.h>

#define THROW_ERROR(...) ScriptManager::Threads[threadID].ThrowRuntimeError(false, __VA_ARGS__)

namespace ImageResource {
	ObjClass* Class = nullptr;

	Uint32 Hash_Width = 0;
	Uint32 Hash_Height = 0;

	bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
		Resourceable* resourceable = (Resourceable*)(((ObjResourceable*)object)->ResourceablePtr);
		if (!resourceable || !resourceable->IsLoaded()) {
			THROW_ERROR("Image is no longer loaded!");
			return true;
		}

		Image* image = (Image*)resourceable;

		if (hash == Hash_Width) {
			*result = INTEGER_VAL((int)image->TexturePtr->Width);
		}
		else if (hash == Hash_Height) {
			*result = INTEGER_VAL((int)image->TexturePtr->Height);
		}
		else {
			return false;
		}

		return true;
	}

	bool VM_PropertySet(Obj* object, Uint32 hash, VMValue result, Uint32 threadID) {
		if (hash == Hash_Width || hash == Hash_Height) {
			THROW_ERROR("Field cannot be written to!");
		}
		else {
			return false;
		}

		return true;
	}

	void Init() {
		const char* className = "ImageResource";

		Class = NewClass(className);

		Hash_Width = Murmur::EncryptString("Width");
		Hash_Height = Murmur::EncryptString("Height");

		TypeImpl::RegisterClass(Class);
		TypeImpl::DefinePrintableName(Class, "image");
	}
};

ObjClass* ResourceableImpl::Class = nullptr;

void ResourceableImpl::Init() {
	Class = NewClass(CLASS_RESOURCEABLE);

	TypeImpl::RegisterClass(Class);

	ImageResource::Init();
}

void* ResourceableImpl::New(void* ptr) {
	ObjResourceable* obj = (ObjResourceable*)AllocateObject(sizeof(ObjResourceable), OBJ_RESOURCEABLE);
	Memory::Track(obj, "NewResourceable");
	obj->Object.Destructor = ResourceableImpl::Dispose;
	obj->ResourceablePtr = ptr;

	Resourceable* resourceable = (Resourceable*)ptr;
	switch (resourceable->Type) {
	case RESOURCE_IMAGE:
		obj->Object.Class = ImageResource::Class;
		obj->Object.PropertyGet = ImageResource::VM_PropertyGet;
		obj->Object.PropertySet = ImageResource::VM_PropertySet;
		break;
	default:
		break;
	}

	return (void*)obj;
}

void ResourceableImpl::Dispose(Obj* object) {
	ObjResourceable* resourceable = (ObjResourceable*)object;

	if (resourceable->ResourceablePtr) {
		((Resourceable*)resourceable->ResourceablePtr)->ReleaseVMObject();
	}
}
