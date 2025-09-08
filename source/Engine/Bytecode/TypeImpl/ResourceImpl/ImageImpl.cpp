#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/ImageImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* ImageImpl::Class = nullptr;

Uint32 Hash_Width = 0;
Uint32 Hash_Height = 0;

#define THROW_ERROR(...) ScriptManager::Threads[threadID].ThrowRuntimeError(false, __VA_ARGS__)

#define GET_RESOURCEABLE(object) (Resourceable*)(((ObjResourceable*)object)->ResourceablePtr)

void ImageImpl::Init() {
	Class = NewClass("ImageResource");

	Hash_Width = Murmur::EncryptString("Width");
	Hash_Height = Murmur::EncryptString("Height");

	TypeImpl::RegisterClass(Class);
	TypeImpl::DefinePrintableName(Class, "image");
}

bool ImageImpl::IsValidField(Uint32 hash) {
	return hash == Hash_Width || hash == Hash_Height;
}

bool ImageImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	if (!IsValidField(hash)) {
		return false;
	}

	Resourceable* resourceable = object ? GET_RESOURCEABLE(object) : nullptr;
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

	return true;
}

bool ImageImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	if (!IsValidField(hash)) {
		return false;
	}

	Resourceable* resourceable = object ? GET_RESOURCEABLE(object) : nullptr;
	if (!resourceable || !resourceable->IsLoaded()) {
		THROW_ERROR("Image is no longer loaded!");
		return true;
	}

	if (hash == Hash_Width || hash == Hash_Height) {
		THROW_ERROR("Field cannot be written to!");
		return true;
	}
	else {
		return false;
	}

	return true;
}
