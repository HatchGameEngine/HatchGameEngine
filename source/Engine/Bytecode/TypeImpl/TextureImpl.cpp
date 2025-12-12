#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/TextureImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>

ObjClass* TextureImpl::Class = nullptr;

void TextureImpl::Init() {
	Class = NewClass(CLASS_TEXTURE);
	Class->NewFn = New;
	Class->Initializer = OBJECT_VAL(NewNative(VM_Initializer));

	ScriptManager::DefineNative(Class, "CopyPixels", VM_CopyPixels);
	ScriptManager::DefineNative(Class, "Apply", VM_Apply);
	ScriptManager::DefineNative(Class, "SetSize", VM_SetSize);
	ScriptManager::DefineNative(Class, "Scale", VM_Scale);
	ScriptManager::DefineNative(Class, "Delete", VM_Delete);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(CLASS_TEXTURE, Class);
	TypeImpl::DefinePrintableName(Class, "texture");
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, StandardLibrary::argFunction) : argDefault)

Obj* TextureImpl::New() {
	ObjTexture* texture = (ObjTexture*)NewNativeInstance(sizeof(ObjTexture));
	Memory::Track(texture, "NewTexture");
	texture->Object.Class = Class;
	texture->Object.Destructor = Dispose;
	return (Obj*)texture;
}
/***
 * \constructor
 * \desc Creates a texture with the given dimensions.
 * \param width (Integer): The width of the texture.
 * \param height (Integer): The height of the texture.
 * \paramOpt access (Enum): The <linkto ref="TEXTUREACCESS_*">access mode</linkto> of the texture. The default is <code>TEXTUREACCESS_STATIC</code>.
 * \paramOpt format (Enum): The <linkto ref="TEXTUREFORMAT_*">format</linkto> of the texture. The default is <code>TEXTUREFORMAT_RGB888</code>, or <code>TextureFormat_NATIVE</code> for render target textures.
 * \paramOpt pixels (Array): An array of pixels to initialize the texture with. The length of the array must match the dimensions of the texture (e.g., for a 64x64 texture, you must pass an array of exactly 4096 values.)
 * \ns Texture
 */
VMValue TextureImpl::VM_Initializer(int argCount, VMValue* args, Uint32 threadID) {
	ObjTexture* objTexture = AS_TEXTURE(args[0]);

	StandardLibrary::CheckAtLeastArgCount(argCount, 3);

	int width = GET_ARG(1, GetInteger);
	int height = GET_ARG(2, GetInteger);
	int access = GET_ARG_OPT(3, GetInteger, TextureAccess_STATIC);
	int format = GET_ARG_OPT(4,
		GetInteger,
		access == TextureAccess_RENDERTARGET ? TextureFormat_NATIVE
						     : TextureFormat_RGB888);
	ObjArray* pixelsArray = GET_ARG_OPT(5, GetArray, nullptr);

	char errorString[128];

	if (width < 1) {
		throw ScriptException("Width cannot be lower than 1.");
	}

	if (height < 1) {
		throw ScriptException("Height cannot be lower than 1.");
	}

	if (access < 0 || access > TextureAccess_RENDERTARGET) {
		snprintf(errorString,
			sizeof errorString,
			"Texture access mode %d out of range. (%d - %d)",
			access,
			0,
			TextureAccess_RENDERTARGET);

		throw ScriptException(errorString);
	}

	if (format == TextureFormat_NATIVE) {
		format = Graphics::TextureFormat;
	}
	else {
		if (access == TextureAccess_RENDERTARGET) {
			throw ScriptException(
				"Texture format of a render target texture must be TEXTUREFORMAT_NATIVE!");
		}

		if (format < 0 || format > TextureFormat_INDEXED) {
			snprintf(errorString,
				sizeof errorString,
				"Texture format %d out of range. (%d - %d)",
				format,
				0,
				TextureFormat_INDEXED);

			throw ScriptException(errorString);
		}
	}

	Uint8* pixels = nullptr;
	unsigned numPaletteColors = 256;

	size_t numPixels = width * height;
	size_t bpp = Texture::GetFormatBytesPerPixel(format);

	if (pixelsArray != nullptr) {
		if (access == TextureAccess_RENDERTARGET) {
			throw ScriptException("Cannot create a render target texture with pixel data!");
		}

		size_t numArrayPixels = pixelsArray->Values->size();

		if (numArrayPixels != numPixels) {
			snprintf(errorString,
				sizeof errorString,
				"Expected array of pixels to have %d elements, but it had %d elements instead.",
				numPixels,
				numArrayPixels);

			throw ScriptException(errorString);
		}

		pixels = (Uint8*)Memory::Calloc(numPixels, bpp);

		Uint8* dest = pixels;
		int pixelFormat = Texture::TextureFormatToPixelFormat(format);

		for (size_t i = 0; i < pixelsArray->Values->size(); i++) {
			VMValue element = (*pixelsArray->Values)[i];
			VMValue result = Value::CastAsInteger(element);

			if (IS_NULL(result)) {
				Memory::Free(pixels);

				snprintf(errorString,
					sizeof errorString,
					"Expected value at index %d to be of type %s; value was of type %s.",
					i,
					GetTypeString(VAL_INTEGER),
					GetValueTypeString(element));

				throw ScriptException(errorString);
			}

			int value = AS_INTEGER(result);

			if (format == TextureFormat_INDEXED) {
				if (value < 0 || value > 255) {
					Memory::Free(pixels);

					snprintf(errorString,
						sizeof errorString,
						"Pixel value %d out of range. (0 - 255)",
						value);

					throw ScriptException(errorString);
				}

				*dest = value;
			}
			else {
				value = TO_BE32((Uint32)value);

#if HATCH_LITTLE_ENDIAN
				if (TEXTUREFORMAT_IS_RGB(format)) {
					value >>= 8;
				}
#endif

				memcpy(dest, &value, bpp);
			}

			dest += bpp;
		}
	}

	Texture* texture = Graphics::CreateTexture(format, access, width, height);

	ScriptManager::RegistryAdd(texture, (Obj*)objTexture);

	if (pixels) {
		Graphics::UpdateTexture(texture, NULL, pixels, texture->Pitch);

		Memory::Free(pixels);
	}

	if (format == TextureFormat_INDEXED) {
		Uint32* palette = (Uint32*)Memory::Calloc(numPaletteColors, sizeof(Uint32));

		for (unsigned i = 0; i < numPaletteColors; i++) {
			Uint8 brightness = (int)(i * std::ceil(255.0 / numPaletteColors));

			palette[i] = ColorUtils::Make(i, i, i, 0xFF, Graphics::PreferredPixelFormat);
		}

		Graphics::SetTexturePalette(texture, palette, numPaletteColors);
	}

	return OBJECT_VAL(objTexture);
}
void TextureImpl::Dispose(Obj* object) {
	// Yes, this leaks memory.
	// Use Delete() in your script for a texture you no longer need!
	InstanceImpl::Dispose(object);
}

void* TextureImpl::GetTexture(ObjTexture* object) {
	return ScriptManager::RegistryGet((Obj*)object);
}
ObjTexture* TextureImpl::GetTextureObject(void* texture) {
	if (texture == nullptr) {
		return nullptr;
	}

	Obj* obj = ScriptManager::RegistryGet(texture);
	if (obj != nullptr) {
		return (ObjTexture*)obj;
	}

	obj = ScriptManager::RegistryAdd(texture, TextureImpl::New());

	return (ObjTexture*)obj;
}

#define CHECK_EXISTS(ptr) \
	if (ptr == nullptr) { \
		throw ScriptException("Texture is no longer valid!"); \
	}

#define CHECK_NOT_TRYING_TO_UPDATE(obj) \
	if (obj->IsViewTexture) { \
		throw ScriptException( \
			"Cannot update a texture that is a draw target belonging to a view!"); \
	}

/***
 * \method CopyPixels
 * \desc Copies pixels from another Texture or Image into the texture.
 * \param srcTexture The texture or image to copy pixels from.
 * \param srcX The X offset of the region to copy.
 * \param srcY The Y offset of the region to copy.
 * \param srcWidth The width of the region to copy.
 * \param srcHeight The height of the region to copy.
 * \param destX The X offset of the destination.
 * \param destY The Y offset of the destination.
 * \ns Texture
 */
VMValue TextureImpl::VM_CopyPixels(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 2);

	ObjTexture* objTexture = AS_TEXTURE(args[0]);
	Texture* srcTexture = GET_ARG(1, GetDrawable);
	int srcX = GET_ARG_OPT(2, GetInteger, 0);
	int srcY = GET_ARG_OPT(3, GetInteger, 0);
	int srcWidth = GET_ARG_OPT(4, GetInteger, srcTexture ? srcTexture->Width : 0);
	int srcHeight = GET_ARG_OPT(5, GetInteger, srcTexture ? srcTexture->Height : 0);
	int destX = GET_ARG_OPT(6, GetInteger, 0);
	int destY = GET_ARG_OPT(7, GetInteger, 0);

	Texture* texture = (Texture*)GetTexture(objTexture);
	CHECK_EXISTS(texture);
	CHECK_NOT_TRYING_TO_UPDATE(objTexture);

	if (srcTexture == nullptr) {
		return NULL_VAL;
	}

	if (!Texture::CanConvertBetweenFormats(srcTexture->Format, texture->Format)) {
		throw ScriptException("Incompatible texture formats!");
	}

	Graphics::CopyTexturePixels(
		texture, destX, destY, srcTexture, srcX, srcY, srcWidth, srcHeight);

	return NULL_VAL;
}
/***
 * \method Apply
 * \desc Uploads all changes made to the texture to the GPU. This is an expensive operation, so change the most amount of pixels as possible before calling this.
 * \ns Texture
 */
VMValue TextureImpl::VM_Apply(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjTexture* objTexture = AS_TEXTURE(args[0]);
	Texture* texture = (Texture*)GetTexture(objTexture);

	CHECK_EXISTS(texture);
	CHECK_NOT_TRYING_TO_UPDATE(objTexture);

	Graphics::UpdateTexture(texture, NULL, texture->Pixels, texture->Pitch);

	return NULL_VAL;
}
/***
 * \method SetSize
 * \desc Changes the dimensions of the texture, without scaling the pixels.
 * \param width (Integer): The new width of the texture.
 * \param height (Integer): The new height of the texture.
 * \ns Texture
 */
VMValue TextureImpl::VM_SetSize(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjTexture* objTexture = AS_TEXTURE(args[0]);
	int width = GET_ARG(1, GetInteger);
	int height = GET_ARG(2, GetInteger);

	Texture* texture = (Texture*)GetTexture(objTexture);
	CHECK_EXISTS(texture);

	if (objTexture->IsViewTexture) {
		throw ScriptException(
			"Cannot resize a draw target texture that belongs to a view!");
	}

	if (width < 1) {
		throw ScriptException("Width cannot be lower than 1.");
	}

	if (height < 1) {
		throw ScriptException("Height cannot be lower than 1.");
	}

	Uint32* pixels = Texture::Crop(texture, 0, 0, width, height);
	if (pixels == nullptr) {
		throw ScriptException("Could not resize texture!");
	}

	Graphics::ResizeTexture(texture, width, height);
	Graphics::UpdateTexture(texture, NULL, pixels, texture->Pitch);

	Memory::Free(pixels);

	return NULL_VAL;
}
/***
 * \method Scale
 * \desc Scales the texture to the given dimensions, using nearest neighbor interpolation.
 * \param width (Integer): The new width of the texture.
 * \param height (Integer): The new height of the texture.
 * \ns Texture
 */
VMValue TextureImpl::VM_Scale(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjTexture* objTexture = AS_TEXTURE(args[0]);
	int width = GET_ARG(1, GetInteger);
	int height = GET_ARG(2, GetInteger);

	Texture* texture = (Texture*)GetTexture(objTexture);
	CHECK_EXISTS(texture);

	if (objTexture->IsViewTexture) {
		throw ScriptException(
			"Cannot scale a draw target texture that belongs to a view!");
	}

	if (width < 1) {
		throw ScriptException("Width cannot be lower than 1.");
	}

	if (height < 1) {
		throw ScriptException("Height cannot be lower than 1.");
	}

	Uint32* pixels = Texture::Scale(texture, width, height);
	if (pixels == nullptr) {
		throw ScriptException("Could not scale texture!");
	}

	Graphics::ResizeTexture(texture, width, height);
	Graphics::UpdateTexture(texture, NULL, pixels, texture->Pitch);

	Memory::Free(pixels);

	return NULL_VAL;
}
/***
 * \method Delete
 * \desc Deletes the texture. It can no longer be used after this function is called.
 * \ns Texture
 */
VMValue TextureImpl::VM_Delete(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjTexture* objTexture = AS_TEXTURE(args[0]);
	Texture* texture = (Texture*)GetTexture(objTexture);

	CHECK_EXISTS(texture);

	if (objTexture->IsViewTexture) {
		throw ScriptException(
			"Cannot delete a draw target texture that belongs to a view!");
	}

	Graphics::DisposeTexture(texture);

	return NULL_VAL;
}

#undef CHECK_EXISTS
#undef CHECK_NOT_TRYING_TO_UPDATE
#undef GET_ARG
#undef GET_ARG_OPT
