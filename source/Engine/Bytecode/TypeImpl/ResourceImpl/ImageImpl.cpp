#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/TypeImpl/AssetImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/ImageImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* ImageImpl::Class = nullptr;

Uint32 Hash_Width = 0;
Uint32 Hash_Height = 0;

#define CLASS_IMAGE "Image"

void ImageImpl::Init() {
	Class = NewClass(CLASS_IMAGE);

	GET_STRING_HASH(Width);
	GET_STRING_HASH(Height);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(CLASS_IMAGE, Class);
	TypeImpl::DefinePrintableName(Class, "image");
}

bool ImageImpl::IsValidField(Uint32 hash) {
	CHECK_VALID_FIELD(Width);
	CHECK_VALID_FIELD(Height);

	return false;
}

bool ImageImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	if (!IsValidField(hash)) {
		return false;
	}

	Asset* asset = object ? GET_ASSET(object) : nullptr;
	if (!asset || !asset->IsLoaded()) {
		VM_THROW_ERROR("Image is no longer loaded!");
		return true;
	}

	Image* image = (Image*)asset;

	/***
	 * \field Width
	 * \desc The width of the image.
	 * \ns Image
 	*/
	if (hash == Hash_Width) {
		*result = INTEGER_VAL((int)image->TexturePtr->Width);
	}
	/***
	 * \field Height
	 * \desc The height of the image.
	 * \ns Image
 	*/
	else if (hash == Hash_Height) {
		*result = INTEGER_VAL((int)image->TexturePtr->Height);
	}

	return true;
}

bool ImageImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	if (!IsValidField(hash)) {
		return false;
	}

	Asset* asset = object ? GET_ASSET(object) : nullptr;
	if (!asset || !asset->IsLoaded()) {
		VM_THROW_ERROR("Image is no longer loaded!");
		return true;
	}

	if (hash == Hash_Width || hash == Hash_Height) {
		VM_THROW_ERROR("Field cannot be written to!");
		return true;
	}

	return false;
}
