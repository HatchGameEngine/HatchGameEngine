#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/TextureImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>

/***
* \class Texture
* \desc Representation of a GPU texture.
*/

ObjClass* TextureImpl::Class = nullptr;

Uint32 Hash_Width = 0;
Uint32 Hash_Height = 0;
Uint32 Hash_Format = 0;
Uint32 Hash_Access = 0;

void TextureImpl::Init() {
	Class = NewClass(CLASS_TEXTURE);
	Class->NewFn = Constructor;
	Class->Initializer = OBJECT_VAL(NewNative(VM_Initializer));

	/***
    * \field Width
    * \type integer
    * \ns Texture
    * \desc The width of the texture.
    */
	Hash_Width = Murmur::EncryptString("Width");
	/***
    * \field Height
    * \type integer
    * \ns Texture
    * \desc The height of the texture.
    */
	Hash_Height = Murmur::EncryptString("Height");
	/***
    * \field Format
    * \type <ref TEXTUREFORMAT_*>
    * \ns Texture
    * \desc The format of the texture.
    */
	Hash_Format = Murmur::EncryptString("Format");
	/***
    * \field Access
    * \type <ref TEXTUREACCESS_*>
    * \ns Texture
    * \desc The access mode of the texture.
    */
	Hash_Access = Murmur::EncryptString("Access");

	ScriptManager::DefineNative(Class, "SetSize", VM_SetSize);
	ScriptManager::DefineNative(Class, "Scale", VM_Scale);
	ScriptManager::DefineNative(Class, "GetPixel", VM_GetPixel);
	ScriptManager::DefineNative(Class, "GetPixelData", VM_GetPixelData);
	ScriptManager::DefineNative(Class, "SetPixel", VM_SetPixel);
	ScriptManager::DefineNative(Class, "CopyPixels", VM_CopyPixels);
	ScriptManager::DefineNative(Class, "Apply", VM_Apply);
	ScriptManager::DefineNative(Class, "Convert", VM_Convert);
	ScriptManager::DefineNative(Class, "Delete", VM_Delete);
	ScriptManager::DefineNative(Class, "CanConvertBetweenFormats", VM_CanConvertBetweenFormats);
	ScriptManager::DefineNative(Class, "FormatHasAlphaChannel", VM_FormatHasAlphaChannel);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(Class);
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, StandardLibrary::argFunction) : argDefault)

Obj* TextureImpl::Constructor() {
	ObjTexture* texture = (ObjTexture*)NewNativeInstance(sizeof(ObjTexture));
	Memory::Track(texture, "NewTexture");
	texture->Object.Class = Class;
	texture->InstanceObj.PropertyGet = VM_PropertyGet;
	texture->InstanceObj.PropertySet = VM_PropertySet;
	texture->InstanceObj.Destructor = Dispose;
	return (Obj*)texture;
}

static void
GetPixelFromArray(ObjArray* pixelsArray, size_t index, int format, size_t bpp, Uint8* dest) {
	char errorString[128];

	VMValue element = (*pixelsArray->Values)[index];
	VMValue result = Value::CastAsInteger(element);

	if (IS_NULL(result)) {
		snprintf(errorString,
			sizeof errorString,
			"Expected value at index %d to be of type %s; value was of type %s.",
			(int)index,
			GetTypeString(VAL_INTEGER),
			GetValueTypeString(element));

		throw ScriptException(errorString);
	}

	int value = AS_INTEGER(result);

	if (format == TextureFormat_INDEXED) {
		if (value < 0 || value > 255) {
			snprintf(errorString,
				sizeof errorString,
				"Pixel value %d out of range. (0 - 255)",
				value);

			throw ScriptException(errorString);
		}

		*dest = value;
	}
	else {
#if HATCH_BIG_ENDIAN
		dest[0] = (value >> 24) & 0xFF;
		dest[1] = (value >> 16) & 0xFF;
		dest[2] = (value >> 8) & 0xFF;
		if (bpp == 4) {
			dest[3] = value & 0xFF;
		}
#else
		if (bpp == 4) {
			dest[0] = (value >> 24) & 0xFF;
			dest[1] = (value >> 16) & 0xFF;
			dest[2] = (value >> 8) & 0xFF;
			dest[3] = value & 0xFF;
		}
		else {
			dest[0] = (value >> 16) & 0xFF;
			dest[1] = (value >> 8) & 0xFF;
			dest[2] = value & 0xFF;
		}
#endif
	}
}
static Uint8* GetPixelDataFromArray(ObjArray* pixelsArray,
	int srcX,
	int srcY,
	int srcWidth,
	int srcHeight,
	int copyWidth,
	int copyHeight,
	int format) {
	char errorString[128];

	size_t numPixels = srcWidth * srcHeight;
	size_t numArrayPixels = pixelsArray->Values->size();

	if (numArrayPixels != numPixels) {
		snprintf(errorString,
			sizeof errorString,
			"Expected array of pixels to have %d elements, but it had %d elements instead.",
			(int)numPixels,
			(int)numArrayPixels);

		throw ScriptException(errorString);
	}

	if (numPixels == 0) {
		return nullptr;
	}

	int x1 = srcX;
	int y1 = srcY;
	int x2 = srcX + copyWidth;
	int y2 = srcY + copyHeight;

	if (x1 < 0 || x2 > srcWidth || y1 < 0 || y2 > srcHeight || copyWidth < 1 ||
		copyHeight < 1) {
		snprintf(errorString,
			sizeof errorString,
			"Invalid region! (X1: %d, Y1: %d, X2: %d, Y2: %d, Width: %d, Height: %d)",
			x1,
			y1,
			x2,
			y2,
			srcWidth,
			srcHeight);

		throw ScriptException(errorString);
	}

	if (x1 == x2 || y1 == y2) {
		return nullptr;
	}

	size_t bpp = Texture::GetFormatBytesPerPixel(format);

	Uint8* pixels = (Uint8*)Memory::Calloc(copyWidth * copyHeight, bpp);
	if (!pixels) {
		throw ScriptException("Out of memory!");
	}

	Uint8* dest = pixels;
	try {
		for (int yy = y1; yy < y2; yy++) {
			for (int xx = x1; xx < x2; xx++) {
				size_t index = (yy * srcWidth) + xx;

				GetPixelFromArray(pixelsArray, index, format, bpp, dest);

				dest += bpp;
			}
		}
	} catch (const ScriptException& error) {
		Memory::Free(pixels);
		throw error;
	}

	return pixels;
}
static Uint8* GetPixelDataFromArray(ObjArray* pixelsArray, int format) {
	size_t numArrayPixels = pixelsArray->Values->size();
	if (numArrayPixels == 0) {
		return nullptr;
	}

	size_t bpp = Texture::GetFormatBytesPerPixel(format);

	Uint8* pixels = (Uint8*)Memory::Calloc(numArrayPixels, bpp);
	if (!pixels) {
		throw ScriptException("Out of memory!");
	}

	Uint8* dest = pixels;
	try {
		for (size_t index = 0; index < numArrayPixels; index++) {
			GetPixelFromArray(pixelsArray, index, format, bpp, dest);
			dest += bpp;
		}
	} catch (const ScriptException& error) {
		Memory::Free(pixels);
		throw error;
	}

	return pixels;
}

static Uint32* GetPaletteFromArray(ObjArray* paletteArray, unsigned& numPaletteColors) {
	char errorString[128];

	numPaletteColors = (unsigned)paletteArray->Values->size();

	if (numPaletteColors == 0) {
		throw ScriptException("Palette must have at least one color.");
	}
	else if (numPaletteColors > 256) {
		snprintf(errorString,
			sizeof errorString,
			"Palette must have a maximum of 256 colors, but it had %d colors.",
			(int)numPaletteColors);

		throw ScriptException(errorString);
	}

	Uint32* palette = (Uint32*)Memory::Malloc(numPaletteColors * sizeof(Uint32));

	for (unsigned i = 0; i < numPaletteColors; i++) {
		VMValue element = (*paletteArray->Values)[i];
		VMValue result = Value::CastAsInteger(element);

		if (IS_NULL(result)) {
			Memory::Free(palette);

			snprintf(errorString,
				sizeof errorString,
				"Expected value at index %d to be of type %s; value was of type %s.",
				(int)i,
				GetTypeString(VAL_INTEGER),
				GetValueTypeString(element));

			throw ScriptException(errorString);
		}

		int value = AS_INTEGER(result);

#if HATCH_BIG_ENDIAN
		Uint8 red = (value >> 24) & 0xFF;
		Uint8 green = (value >> 16) & 0xFF;
		Uint8 blue = (value >> 8) & 0xFF;
#else
		Uint8 red = (value >> 16) & 0xFF;
		Uint8 green = (value >> 8) & 0xFF;
		Uint8 blue = value & 0xFF;
#endif

		palette[i] =
			ColorUtils::Make(red, green, blue, 0xFF, Graphics::PreferredPixelFormat);
	}

	return palette;
}

#define VALIDATE_TEXTURE_FORMAT(textureFormat) \
	{ \
		if (textureFormat == TextureFormat_NATIVE) { \
			textureFormat = Graphics::TextureFormat; \
		} \
		else if (textureFormat < 0 || textureFormat > TextureFormat_INDEXED) { \
			char errorString[128]; \
			snprintf(errorString, \
				sizeof errorString, \
				"Texture format %d out of range. (0 - %d)", \
				textureFormat, \
				TextureFormat_INDEXED); \
			throw ScriptException(errorString); \
		} \
	}

/***
 * \constructor
 * \desc Creates a texture with the given dimensions, access mode, and format.
 * \param width (integer): The width of the texture.
 * \param height (integer): The height of the texture.
 * \paramOpt access (<ref TEXTUREACCESS_*>): The access mode of the texture. (default: <ref TEXTUREACCESS_STATIC>)
 * \paramOpt format (<ref TEXTUREFORMAT_*>): The format of the texture. The default is <ref TEXTUREFORMAT_RGB888>, or <ref TEXTUREFORMAT_NATIVE> for render target textures.
 * \paramOpt pixels (array): An array of pixels to initialize the texture with. The length of the array must match the dimensions of the texture (e.g., for a 64x64 texture, you must pass an array of exactly 4096 values.)
 * \paramOpt palette (array): The palette of the texture, format 0xRRGGBB.
 * \ns Texture
 */
/***
 * \constructor
 * \desc Creates a texture with the given dimensions, access mode, and format.
 * \param width (integer): The width of the texture.
 * \param height (integer): The height of the texture.
 * \paramOpt access (<ref TEXTUREACCESS_*>): The access mode of the texture. (default: <ref TEXTUREACCESS_STATIC>)
 * \paramOpt format (<ref TEXTUREFORMAT_*>): The format of the texture. The default is <ref TEXTUREFORMAT_RGB888>, or <ref TEXTUREFORMAT_NATIVE> for render target textures.
 * \paramOpt source (Drawable): The Drawable to initialize the texture with.
 * \paramOpt palette (array): The palette of the Drawable, format 0xRRGGBB.
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
		access == TextureAccess_RENDERTARGET ? TextureFormat_NATIVE : TextureFormat_RGB888);
	Texture* srcTexture = nullptr;
	ObjArray* pixelsArray = nullptr;
	ObjArray* paletteArray = nullptr;

	char errorString[128];

	if (width < 1) {
		throw ScriptException("Width cannot be less than 1.");
	}

	if (height < 1) {
		throw ScriptException("Height cannot be less than 1.");
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

		VALIDATE_TEXTURE_FORMAT(format);
	}

	Uint8* pixels = nullptr;
	Uint32* palette = nullptr;
	Uint32* paletteCopy = nullptr;
	unsigned numPaletteColors = 0;
	bool doIndexedConversion = false;

	if (argCount >= 6 && !IS_NULL(args[5])) {
		if (IS_ARRAY(args[5])) {
			pixelsArray = GET_ARG(5, GetArray);
		}
		else {
			srcTexture = GET_ARG(5, GetDrawable);
		}
	}

	if (argCount >= 7) {
		paletteArray = GET_ARG(6, GetArray);
	}

	if (srcTexture || pixelsArray) {
		if (access == TextureAccess_RENDERTARGET) {
			throw ScriptException(
				"Cannot create a render target texture with pixel data!");
		}

		if (pixelsArray) {
			pixels = GetPixelDataFromArray(
				pixelsArray, 0, 0, width, height, width, height, format);
		}
		// If the formats don't match
		else if (format != srcTexture->Format) {
			// If either format is indexed
			if (format == TextureFormat_INDEXED ||
				srcTexture->Format == TextureFormat_INDEXED) {
				doIndexedConversion = true;
			}
			else if (!Texture::CanConvertBetweenFormats(format, srcTexture->Format)) {
				throw ScriptException("Incompatible texture formats!");
			}
		}
	}
	else if (paletteArray && format != TextureFormat_INDEXED) {
		throw ScriptException("Cannot create a non-indexed texture with a palette!");
	}

	// Determine which palette to use
	if (paletteArray) {
		try {
			paletteCopy = GetPaletteFromArray(paletteArray, numPaletteColors);
		} catch (const ScriptException& error) {
			Memory::Free(pixels);
			throw error;
		}

		palette = paletteCopy;
	}
	else if (srcTexture) {
		palette = srcTexture->PaletteColors;
		numPaletteColors = srcTexture->NumPaletteColors;
	}

	// If no palette was given when one is needed, create a grayscale palette of 256 colors
	if (!palette && format == TextureFormat_INDEXED) {
		numPaletteColors = 256;
		paletteCopy = (Uint32*)Memory::Malloc(numPaletteColors * sizeof(Uint32));
		if (!paletteCopy) {
			Memory::Free(pixels);
			throw ScriptException(
				"Could not create a default palette for the texture!");
		}

		palette = paletteCopy;

		for (unsigned i = 0; i < numPaletteColors; i++) {
			Uint8 brightness = (int)(i * std::ceil(255.0 / numPaletteColors));

			palette[i] =
				ColorUtils::Make(i, i, i, 0xFF, Graphics::PreferredPixelFormat);
		}
	}

	// Check if pixels need to be converted between indexed and non-indexed formats
	if (doIndexedConversion) {
		if (format == TextureFormat_INDEXED) {
			int transparentIndex =
				Graphics::GetPaletteTransparentColor(palette, numPaletteColors);
			if (transparentIndex == -1) {
				transparentIndex = 0;
			}

			pixels = Texture::GetPalettizedPixels(srcTexture->Pixels,
				srcTexture->Format,
				srcTexture->Width,
				srcTexture->Height,
				palette,
				numPaletteColors,
				transparentIndex);
		}
		else {
			// The source texture is indexed
			pixels = (Uint8*)Texture::GetNonIndexedPixels(srcTexture->Pixels,
				srcTexture->Width,
				srcTexture->Height,
				format,
				palette,
				numPaletteColors);
		}

		if (!pixels) {
			Memory::Free(paletteCopy);
			throw ScriptException("Could not convert pixels!");
		}
	}

	Texture* texture = Graphics::CreateTexture(format, access, width, height);
	if (!texture) {
		Memory::Free(pixels);
		Memory::Free(paletteCopy);
		return NULL_VAL;
	}

	ScriptManager::RegistryAdd(texture, (Obj*)objTexture);

	if (srcTexture) {
		if (doIndexedConversion) {
			texture->CopyPixels(pixels,
				texture->Format,
				0,
				0,
				srcTexture->Width,
				srcTexture->Height,
				0,
				0,
				width,
				height);
		}
		else {
			Graphics::CopyTexturePixels(texture,
				0,
				0,
				srcTexture,
				0,
				0,
				srcTexture->Width,
				srcTexture->Height);
		}

		// Update the texture
		Graphics::UpdateTexture(texture, NULL, texture->Pixels, texture->Pitch);
	}
	else if (pixels) {
		// Copy the pixels and update the texture
		Graphics::UpdateTexture(texture, NULL, pixels, texture->Pitch);
	}

	Memory::Free(pixels);

	// Define the palette
	if (format == TextureFormat_INDEXED) {
		// If no palette was given but a source texture was given, copy the palette of the source
		if (!paletteCopy) {
			paletteCopy = (Uint32*)Memory::Malloc(numPaletteColors * sizeof(Uint32));
			memcpy(paletteCopy, palette, numPaletteColors * sizeof(Uint32));
		}

		Graphics::SetTexturePalette(texture, paletteCopy, numPaletteColors);
	}

	return OBJECT_VAL(objTexture);
}
void TextureImpl::Dispose(Obj* object) {
	// Yes, this leaks memory.
	// Use Delete() in your script for a texture you no longer need!
	InstanceImpl::Dispose(object);
}

bool TextureImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	ObjTexture* objTexture = (ObjTexture*)object;
	Texture* texture = (Texture*)GetTexture(objTexture);
	if (texture == nullptr) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Texture is no longer valid!");
		return false;
	}

	if (hash == Hash_Width) {
		if (result) {
			*result = INTEGER_VAL((int)texture->Width);
		}
		return true;
	}
	else if (hash == Hash_Height) {
		if (result) {
			*result = INTEGER_VAL((int)texture->Height);
		}
		return true;
	}
	else if (hash == Hash_Format) {
		if (result) {
			*result = INTEGER_VAL((int)texture->Format);
		}
		return true;
	}
	else if (hash == Hash_Access) {
		if (result) {
			*result = INTEGER_VAL((int)texture->Access);
		}
		return true;
	}

	return false;
}

bool TextureImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	ObjTexture* objTexture = (ObjTexture*)object;
	Texture* texture = (Texture*)GetTexture(objTexture);
	if (texture == nullptr) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Texture is no longer valid!");
		return false;
	}

#define CHECK_CANNOT_MODIFY(name) \
	{ \
		if (hash == Hash_##name) { \
			ScriptManager::Threads[threadID].ThrowRuntimeError( \
				false, "Field \"" #name "\" cannot be written to!"); \
			return true; \
		} \
	}

	CHECK_CANNOT_MODIFY(Width)
	CHECK_CANNOT_MODIFY(Height)
	CHECK_CANNOT_MODIFY(Format)
	CHECK_CANNOT_MODIFY(Access)

#undef CHECK_CANNOT_MODIFY

	return false;
}

void* TextureImpl::GetTexture(ObjTexture* object) {
	return ScriptManager::RegistryGet((Obj*)object);
}
ObjTexture* TextureImpl::GetTextureObject(void* texture, bool isViewTexture) {
	if (texture == nullptr) {
		return nullptr;
	}

	Obj* obj = ScriptManager::RegistryGet(texture);
	if (obj != nullptr) {
		return (ObjTexture*)obj;
	}

	obj = ScriptManager::RegistryAdd(texture, TextureImpl::Constructor());

	ObjTexture* textureObj = (ObjTexture*)obj;
	textureObj->IsViewTexture = isViewTexture;
	return textureObj;
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
 * \method SetSize
 * \desc Changes the dimensions of the texture, without scaling the pixels.
 * \param width (integer): The new width of the texture.
 * \param height (integer): The new height of the texture.
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
		throw ScriptException("Width cannot be less than 1.");
	}

	if (height < 1) {
		throw ScriptException("Height cannot be less than 1.");
	}

	void* pixels = Texture::Crop(texture, 0, 0, width, height);
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
 * \param width (integer): The new width of the texture.
 * \param height (integer): The new height of the texture.
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
		throw ScriptException("Cannot scale a draw target texture that belongs to a view!");
	}

	if (width < 1) {
		throw ScriptException("Width cannot be less than 1.");
	}

	if (height < 1) {
		throw ScriptException("Height cannot be less than 1.");
	}

	void* pixels = (Uint8*)Texture::GetScaledPixels(
		texture, 0, 0, texture->Width, texture->Height, width, height, texture->Format);
	if (pixels == nullptr) {
		throw ScriptException("Could not scale texture!");
	}

	Graphics::ResizeTexture(texture, width, height);
	Graphics::UpdateTexture(texture, NULL, pixels, texture->Pitch);

	Memory::Free(pixels);

	return NULL_VAL;
}
#define CONVERT_TEXTURE_PIXEL(px, textureFormat) \
	{ \
		if (TEXTUREFORMAT_IS_RGB(textureFormat)) { \
			int red = px & 0xFF; \
			int green = (px >> 8) & 0xFF; \
			int blue = (px >> 16) & 0xFF; \
			px = (red << 16) | (green << 8) | blue; \
		} \
		else if (TEXTUREFORMAT_IS_RGBA(textureFormat)) { \
			int red = px & 0xFF; \
			int green = (px >> 8) & 0xFF; \
			int blue = (px >> 16) & 0xFF; \
			int alpha = (px >> 24) & 0xFF; \
			px = (red << 24) | (green << 16) | (blue << 8) | alpha; \
		} \
	}
/***
 * \method GetPixel
 * \desc Gets the pixel at the specified coordinates.
 * \param x (integer): The X coordinate of the pixel to get.
 * \param y (integer): The Y coordinate of the pixel to get.
 * \return integer Returns a color in the pixel format of the texture.
 * \ns Texture
 */
VMValue TextureImpl::VM_GetPixel(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ObjTexture* objTexture = AS_TEXTURE(args[0]);
	int x = GET_ARG(1, GetInteger);
	int y = GET_ARG(2, GetInteger);

	Texture* texture = (Texture*)GetTexture(objTexture);
	CHECK_EXISTS(texture);

	if (texture->Access == TextureAccess_RENDERTARGET) {
		throw ScriptException("Cannot directly get a pixel of a draw target texture!");
	}

	if (x < 0 || x >= texture->Width || y < 0 || y >= texture->Height) {
		char errorString[128];

		snprintf(errorString,
			sizeof errorString,
			"Invalid coordinates! (X: %d, Y: %d, Width: %d, Height: %d)",
			x,
			y,
			texture->Width,
			texture->Height);

		throw ScriptException(errorString);
	}

	int pixel = texture->GetPixel(x, y);

#if HATCH_LITTLE_ENDIAN
	CONVERT_TEXTURE_PIXEL(pixel, texture->Format);
#endif

	return INTEGER_VAL(pixel);
}
/***
 * \method GetPixelData
 * \desc Gets an array of pixels of the given dimensions at the specified coordinates.
 * \paramOpt x (integer): The X coordinate of the region. (default: `0`)
 * \paramOpt y (integer): The Y coordinate of the region. (default: `0`)
 * \paramOpt width (integer): The width of the region. (default: width of texture)
 * \paramOpt height (integer): The height of the region. (default: height of texture)
 * \return array Returns an array of pixels in the pixel format of the texture.
 * \ns Texture
 */
VMValue TextureImpl::VM_GetPixelData(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 1);

	ObjTexture* objTexture = AS_TEXTURE(args[0]);
	int x = GET_ARG_OPT(1, GetInteger, 0);
	int y = GET_ARG_OPT(2, GetInteger, 0);
	int width, height;

	Texture* texture = (Texture*)GetTexture(objTexture);
	CHECK_EXISTS(texture);

	if (texture->Access == TextureAccess_RENDERTARGET) {
		throw ScriptException("Cannot directly get the pixels of a draw target texture!");
	}

	if (argCount >= 4) {
		width = GET_ARG(3, GetInteger);

		if (width < 0) {
			throw ScriptException("Width cannot be less than zero.");
		}
	}
	else {
		width = texture->Width;
	}
	if (argCount >= 5) {
		height = GET_ARG(4, GetInteger);

		if (height < 0) {
			throw ScriptException("Height cannot be less than zero.");
		}
	}
	else {
		height = texture->Height;
	}

	int x1 = x;
	int y1 = y;
	int x2 = x + width;
	int y2 = y + height;

	if (x1 == x2 || y1 == y2) {
		return OBJECT_VAL(NewArray());
	}

	if (x1 < 0 || x2 > texture->Width || y1 < 0 || y2 > texture->Height) {
		char errorString[128];

		snprintf(errorString,
			sizeof errorString,
			"Invalid region! (X1: %d, Y1: %d, X2: %d, Y2: %d, Width: %d, Height: %d)",
			x1,
			y1,
			x2,
			y2,
			width,
			height);

		throw ScriptException(errorString);
	}

	ObjArray* array = NewArray();

	for (y = y1; y < y2; y++) {
		for (x = x1; x < x2; x++) {
			int pixel = texture->GetPixel(x, y);

#if HATCH_LITTLE_ENDIAN
			CONVERT_TEXTURE_PIXEL(pixel, texture->Format);
#endif

			array->Values->push_back(INTEGER_VAL(pixel));
		}
	}

	return OBJECT_VAL(array);
}
/***
 * \method SetPixel
 * \desc Sets the pixel at the specified coordinates.
 * \param x (integer): The X coordinate of the pixel to set.
 * \param y (integer): The Y coordinate of the pixel to set.
 * \param color (integer): The color in the pixel format of the texture.
 * \ns Texture
 */
VMValue TextureImpl::VM_SetPixel(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 4);

	ObjTexture* objTexture = AS_TEXTURE(args[0]);
	int x = GET_ARG(1, GetInteger);
	int y = GET_ARG(2, GetInteger);
	int color = GET_ARG(3, GetInteger);

	char errorString[128];

	Texture* texture = (Texture*)GetTexture(objTexture);
	CHECK_EXISTS(texture);
	CHECK_NOT_TRYING_TO_UPDATE(objTexture);

	if (texture->Access == TextureAccess_RENDERTARGET) {
		throw ScriptException("Cannot directly change a pixel of a draw target texture!");
	}

	if (x < 0 || x >= texture->Width || y < 0 || y >= texture->Height) {
		snprintf(errorString,
			sizeof errorString,
			"Invalid coordinates! (X: %d, Y: %d, Width: %d, Height: %d)",
			x,
			y,
			texture->Width,
			texture->Height);

		throw ScriptException(errorString);
	}

	if (texture->Format == TextureFormat_INDEXED && (color < 0 || color > 255)) {
		snprintf(errorString,
			sizeof errorString,
			"Pixel value %d out of range. (0 - 255)",
			color);

		throw ScriptException(errorString);
	}

#if HATCH_BIG_ENDIAN
	CONVERT_TEXTURE_PIXEL(color, texture->Format);
#endif

	texture->SetPixel(x, y, color);

	return NULL_VAL;
}
/***
 * \method CopyPixels
 * \desc Copies pixels from a Drawable into the specified coordinates. The formats must be compatible.
 * \param srcDrawable (Drawable): The Drawable to copy pixels from.
 * \paramOpt srcPalette (array): The palette of the source, format 0xRRGGBB.
 * \ns Texture
 */
/***
 * \method CopyPixels
 * \desc Copies pixels from a Drawable into the specified coordinates. The formats must be compatible.
 * \param srcDrawable (Drawable): The Drawable to copy pixels from.
 * \param destX (integer): The X coordinate in the destination to copy the pixels to.
 * \param destY (integer): The Y coordinate in the destination to copy the pixels to.
 * \paramOpt srcPalette (array): The palette of the source, format 0xRRGGBB.
 * \ns Texture
 */
/***
 * \method CopyPixels
 * \desc Copies a region of pixels from a Drawable into the specified coordinates. The formats must be compatible.
 * \param srcDrawable (Drawable): The Drawable to copy pixels from.
 * \param srcX (integer): The X coordinate in the source to start copying the pixels from.
 * \param srcY (integer): The Y coordinate in the source to start copying the pixels from.
 * \param destX (integer): The X coordinate in the destination to copy the pixels to.
 * \param destY (integer): The Y coordinate in the destination to copy the pixels to.
 * \param destWidth (integer): The width of the region to copy.
 * \param destHeight (integer): The height of the region to copy.
 * \paramOpt srcPalette (array): The palette of the source, format 0xRRGGBB.
 * \ns Texture
 */
/***
 * \method CopyPixels
 * \desc Copies pixels from an array into the specified coordinates.
 * \param srcPixels (array): An array of pixels in the pixel format of the texture.
 * \paramOpt srcPalette (array): The palette of the source, format 0xRRGGBB.
 * \ns Texture
 */
/***
 * \method CopyPixels
 * \desc Copies pixels from an array into the specified coordinates.
 * \param srcPixels (array): An array of pixels in the pixel format of the texture.
 * \param srcFormat (<ref TEXTUREFORMAT_*>): The format of the pixels in the array.
 * \paramOpt srcPalette (array): The palette of the source, format 0xRRGGBB. Required if <param srcFormat> is `TEXTUREFORMAT_INDEXED`, and the format of the destination texture is not `TEXTUREFORMAT_INDEXED`.
 * \ns Texture
 */
/***
 * \method CopyPixels
 * \desc Copies pixels from an array into the specified coordinates.
 * \param srcPixels (array): An array of pixels in the pixel format of the texture.
 * \param srcFormat (<ref TEXTUREFORMAT_*>): The format of the pixels in the array.
 * \param srcWidth (integer): The width of the source.
 * \param srcHeight (integer): The height of the source.
 * \paramOpt srcPalette (array): The palette of the source, format 0xRRGGBB. Required if <param srcFormat> is `TEXTUREFORMAT_INDEXED`, and the format of the destination texture is not `TEXTUREFORMAT_INDEXED`.
 * \ns Texture
 */
/***
 * \method CopyPixels
 * \desc Copies a region of pixels from an array into the specified coordinates.
 * \param srcPixels (array): An array of pixels in the pixel format of the texture.
 * \param srcFormat (<ref TEXTUREFORMAT_*>): The format of the pixels in the array.
 * \param srcWidth (integer): The width of the source.
 * \param srcHeight (integer): The height of the source.
 * \param destX (integer): The X coordinate in the destination to copy the pixels to.
 * \param destY (integer): The Y coordinate in the destination to copy the pixels to.
 * \paramOpt srcPalette (array): The palette of the source, format 0xRRGGBB. Required if <param srcFormat> is `TEXTUREFORMAT_INDEXED`, and the format of the destination texture is not `TEXTUREFORMAT_INDEXED`.
 * \ns Texture
 */
/***
 * \method CopyPixels
 * \desc Copies a region of pixels from an array into the specified coordinates.
 * \param srcPixels (array): An array of pixels in the pixel format of the texture.
 * \param srcFormat (<ref TEXTUREFORMAT_*>): The format of the pixels in the array.
 * \param srcX (integer): The X coordinate in the source to start copying the pixels from.
 * \param srcY (integer): The Y coordinate in the source to start copying the pixels from.
 * \param srcWidth (integer): The width of the source.
 * \param srcHeight (integer): The height of the source.
 * \param destX (integer): The X coordinate in the destination to copy the pixels to.
 * \param destY (integer): The Y coordinate in the destination to copy the pixels to.
 * \param destWidth (integer): The width of the region to copy.
 * \param destHeight (integer): The height of the region to copy.
 * \paramOpt srcPalette (array): The palette of the source, format 0xRRGGBB. Required if <param srcFormat> is `TEXTUREFORMAT_INDEXED`, and the format of the destination texture is not `TEXTUREFORMAT_INDEXED`.
 * \ns Texture
 */
VMValue TextureImpl::VM_CopyPixels(int argCount, VMValue* args, Uint32 threadID) {
	if (argCount < 2) {
		StandardLibrary::CheckArgCount(argCount, 2);
		return NULL_VAL;
	}

	ObjArray* paletteArray = nullptr;
	if (argCount >= 3 && IS_ARRAY(args[argCount - 1])) {
		paletteArray = GET_ARG(argCount - 1, GetArray);
		argCount--;
	}

	if (IS_ARRAY(args[1])) {
		if (argCount != 2 && argCount != 3 && argCount != 5 && argCount != 7 &&
			argCount != 11) {
			StandardLibrary::CheckArgCount(argCount, 2);
			return NULL_VAL;
		}
	}
	else {
		if (argCount != 2 && argCount != 4 && argCount != 8) {
			StandardLibrary::CheckArgCount(argCount, 2);
			return NULL_VAL;
		}
	}

	ObjTexture* objTexture = AS_TEXTURE(args[0]);
	Texture* srcTexture = nullptr;
	ObjArray* srcPixelsArray = nullptr;
	int srcFormat;
	int srcWidth, srcHeight;
	bool doIndexedConversion = false;

	char errorString[128];

	Texture* texture = (Texture*)GetTexture(objTexture);
	CHECK_EXISTS(texture);
	CHECK_NOT_TRYING_TO_UPDATE(objTexture);

	if (texture->Access == TextureAccess_RENDERTARGET) {
		throw ScriptException(
			"Cannot directly change the pixels of a draw target texture!");
	}

	int destX = 0, destY = 0;
	int srcX = 0, srcY = 0;
	int destWidth, destHeight;

	if (IS_ARRAY(args[1])) {
		srcPixelsArray = GET_ARG(1, GetArray);
		if (!srcPixelsArray) {
			return NULL_VAL;
		}

		if (argCount >= 3) {
			srcFormat = GET_ARG(2, GetInteger);

			VALIDATE_TEXTURE_FORMAT(srcFormat);
		}
		else {
			srcFormat = texture->Format;
		}

		if (argCount == 11) {
			srcX = GET_ARG(3, GetInteger);
			srcY = GET_ARG(4, GetInteger);

			srcWidth = GET_ARG(5, GetInteger);
			srcHeight = GET_ARG(6, GetInteger);

			destX = GET_ARG(7, GetInteger);
			destY = GET_ARG(8, GetInteger);

			destWidth = GET_ARG(9, GetInteger);
			destHeight = GET_ARG(10, GetInteger);
		}
		else if (argCount >= 5) {
			srcWidth = GET_ARG(3, GetInteger);
			srcHeight = GET_ARG(4, GetInteger);

			if (argCount == 7) {
				destX = GET_ARG(5, GetInteger);
				destY = GET_ARG(6, GetInteger);
			}

			destWidth = srcWidth;
			destHeight = srcHeight;
		}
		else {
			srcWidth = destWidth = texture->Width;
			srcHeight = destHeight = texture->Height;
		}

		if (srcWidth < 0) {
			throw ScriptException("Width of the source cannot be less than zero.");
		}
		if (srcHeight < 0) {
			throw ScriptException("Height of the source cannot be less than zero.");
		}
	}
	else {
		srcTexture = GET_ARG(1, GetDrawable);
		if (srcTexture == nullptr) {
			return NULL_VAL;
		}

		srcFormat = srcTexture->Format;
		srcWidth = srcTexture->Width;
		srcHeight = srcTexture->Height;

		if (argCount == 8) {
			srcX = GET_ARG(2, GetInteger);
			srcY = GET_ARG(3, GetInteger);

			destX = GET_ARG(4, GetInteger);
			destY = GET_ARG(5, GetInteger);

			destWidth = GET_ARG(6, GetInteger);
			destHeight = GET_ARG(7, GetInteger);
		}
		else {
			if (argCount == 4) {
				destX = GET_ARG(2, GetInteger);
				destY = GET_ARG(3, GetInteger);
			}

			destWidth = srcWidth;
			destHeight = srcHeight;
		}
	}

	// If the formats don't match
	if (srcFormat != texture->Format) {
		// If either format is indexed
		if (srcFormat == TextureFormat_INDEXED ||
			texture->Format == TextureFormat_INDEXED) {
			doIndexedConversion = true;
		}
		else if (!Texture::CanConvertBetweenFormats(srcFormat, texture->Format)) {
			throw ScriptException("Incompatible texture formats!");
		}
	}

	if (srcWidth == 0 || srcHeight == 0) {
		return NULL_VAL;
	}

	if (destWidth < 0) {
		throw ScriptException("Width of the region to copy cannot be less than zero.");
	}
	else if (destWidth > srcWidth) {
		snprintf(errorString,
			sizeof errorString,
			"Width of the region to copy cannot be higher than %d.",
			srcWidth);

		throw ScriptException(errorString);
	}
	if (destHeight < 0) {
		throw ScriptException("Height of the region to copy cannot be less than zero.");
	}
	else if (destHeight > srcHeight) {
		snprintf(errorString,
			sizeof errorString,
			"Height of the region to copy cannot be higher than %d.",
			srcHeight);

		throw ScriptException(errorString);
	}

	if (destWidth == 0 || destHeight == 0) {
		return NULL_VAL;
	}

	Uint32* srcPalette = nullptr;
	Uint32* paletteFromArray = nullptr;
	unsigned numPaletteColors = 0;
	int transparentIndex = 0;

	// If converting FROM indexed, it uses the source texture's palette, or the srcPalette
	// parameter, which is mandatory if the source is an array.
	// If converting TO indexed, it uses the srcPalette parameter, or the palette of the
	// destination texture.
	// If both formats are indexed, it copies the indices as-is.
	if (doIndexedConversion) {
		if (paletteArray) {
			paletteFromArray = GetPaletteFromArray(paletteArray, numPaletteColors);
		}

		if (paletteFromArray) {
			srcPalette = paletteFromArray;
		}
		else {
			if (srcTexture && srcTexture->PaletteColors) {
				srcPalette = srcTexture->PaletteColors;
				numPaletteColors = srcTexture->NumPaletteColors;
			}
			else {
				srcPalette = texture->PaletteColors;
				numPaletteColors = texture->NumPaletteColors;
			}
		}

		if (!srcPalette) {
			throw ScriptException("Must specify palette!");
		}

		transparentIndex =
			Graphics::GetPaletteTransparentColor(srcPalette, numPaletteColors);
		if (transparentIndex == -1) {
			transparentIndex = 0;
		}
	}

	if (srcPixelsArray) {
		Uint8* pixels = nullptr;

		if (ScriptManager::Lock()) {
			try {
				pixels = GetPixelDataFromArray(srcPixelsArray,
					srcX,
					srcY,
					srcWidth,
					srcHeight,
					destWidth,
					destHeight,
					srcFormat);
			} catch (const ScriptException& error) {
				Memory::Free(paletteFromArray);
				ScriptManager::Unlock();
				throw error;
			}
			ScriptManager::Unlock();
		}

		int copyFormat = srcFormat;

		if (doIndexedConversion) {
			void* result;

			// The target texture is indexed
			if (texture->Format == TextureFormat_INDEXED) {
				result = Texture::GetPalettizedPixels(pixels,
					srcFormat,
					destWidth,
					destHeight,
					srcPalette,
					numPaletteColors,
					transparentIndex);
			}
			else {
				// The source array is indexed
				result = Texture::GetNonIndexedPixels(pixels,
					destWidth,
					destHeight,
					texture->Format,
					srcPalette,
					numPaletteColors);
			}

			Memory::Free(pixels);

			if (!result) {
				Memory::Free(paletteFromArray);
				throw ScriptException("Could not convert pixels!");
			}

			pixels = (Uint8*)result;
			copyFormat = texture->Format;
		}

		texture->CopyPixels(pixels,
			copyFormat,
			0,
			0,
			destWidth,
			destHeight,
			destX,
			destY,
			destWidth,
			destHeight);

		Memory::Free(pixels);
	}
	else {
		if (doIndexedConversion) {
			int copyWidth, copyHeight;
			void* region = srcTexture->GetRegion(
				srcX, srcY, destWidth, destHeight, &copyWidth, &copyHeight);

			// If there's nothing to copy, GetRegion returns nullptr, which is fine.
			if (region) {
				void* result;

				// The target texture is indexed
				if (texture->Format == TextureFormat_INDEXED) {
					result = Texture::GetPalettizedPixels(region,
						srcTexture->Format,
						copyWidth,
						copyHeight,
						paletteFromArray ? paletteFromArray
								 : texture->PaletteColors,
						paletteFromArray ? numPaletteColors
								 : texture->NumPaletteColors,
						transparentIndex);
				}
				else {
					// The source texture is indexed
					result = Texture::GetNonIndexedPixels(region,
						copyWidth,
						copyHeight,
						texture->Format,
						srcPalette,
						numPaletteColors);
				}

				Memory::Free(region);

				if (!result) {
					Memory::Free(paletteFromArray);
					throw ScriptException("Could not convert pixels!");
				}

				texture->CopyPixels(result,
					texture->Format,
					0,
					0,
					copyWidth,
					copyHeight,
					destX,
					destY,
					copyWidth,
					copyHeight);

				Memory::Free(result);
			}
		}
		else {
			Graphics::CopyTexturePixels(texture,
				destX,
				destY,
				srcTexture,
				srcX,
				srcY,
				destWidth,
				destHeight);
		}
	}

	Memory::Free(paletteFromArray);

	return NULL_VAL;
}
/***
 * Texture.Convert
 * \desc Converts the format of the pixels in <param srcPixels> from <param srcFormat> into <param targetFormat>. If <param srcFormat> and <param targetFormat> are the same, this just copies the pixels from <param srcDrawable> into <param destPixels>.
 * \param srcDrawable (Drawable): The Drawable to convert.
 * \param destPixels (array): The destination array.
 * \param targetFormat (<ref TEXTUREFORMAT_*>): The target format.
 * \paramOpt palette (array): The palette of the source if <param srcFormat> is `TEXTUREFORMAT_INDEXED`, or the palette of the target if <param targetFormat> is `TEXTUREFORMAT_INDEXED`. Format 0xRRGGBB.
 * \ns Texture
 */
/***
 * Texture.Convert
 * \desc Converts the format of the pixels in <param srcPixels> from <param srcFormat> into <param targetFormat>. If <param srcFormat> and <param targetFormat> are the same, this just copies the pixels from <param srcPixels> into <param destPixels>.
 * \param srcPixels (array): An array of pixels.
 * \param srcFormat (<ref TEXTUREFORMAT_*>): The source format.
 * \param destPixels (array): The destination array.
 * \param targetFormat (<ref TEXTUREFORMAT_*>): The target format.
 * \paramOpt palette (array): The palette of the source if <param srcFormat> is `TEXTUREFORMAT_INDEXED`, or the palette of the target if <param targetFormat> is `TEXTUREFORMAT_INDEXED`. Format 0xRRGGBB. Required if <param srcFormat> is `TEXTUREFORMAT_INDEXED`.
 * \ns Texture
 */
/***
 * Texture.Convert
 * \desc Converts <param color> from <param srcFormat> into <param targetFormat>. If <param srcFormat> and <param targetFormat> are the same, this just returns <param color>.
 * \param color (integer): The color to convert.
 * \param srcFormat (<ref TEXTUREFORMAT_*>): The source format.
 * \param targetFormat (<ref TEXTUREFORMAT_*>): The target format.
 * \paramOpt palette (array): The palette of the source if <param srcFormat> is `TEXTUREFORMAT_INDEXED`, or the palette of the target if <param targetFormat> is `TEXTUREFORMAT_INDEXED`. Format 0xRRGGBB. Required if <param srcFormat> is `TEXTUREFORMAT_INDEXED`.
 * \return integer Returns <param color> converted to <param targetFormat>.
 * \ns Texture
 */
VMValue TextureImpl::VM_Convert(int argCount, VMValue* args, Uint32 threadID) {
	if (argCount > 0 && IS_ARRAY(args[0])) {
		StandardLibrary::CheckAtLeastArgCount(argCount, 4);
	}
	else {
		StandardLibrary::CheckAtLeastArgCount(argCount, 3);
	}

	ObjArray* srcPixelsArray = nullptr;
	Texture* srcTexture = nullptr;
	VMValue srcColor = NULL_VAL;
	int srcFormat;
	ObjArray* destPixelsArray = nullptr;
	int targetFormat;
	ObjArray* paletteArray = nullptr;

	if (IS_INTEGER(args[0])) {
		srcColor = args[0];
		srcFormat = GET_ARG(1, GetInteger);
		targetFormat = GET_ARG(2, GetInteger);
		paletteArray = GET_ARG_OPT(3, GetArray, nullptr);

		VALIDATE_TEXTURE_FORMAT(srcFormat);
		VALIDATE_TEXTURE_FORMAT(targetFormat);

		// Easy
		if (srcFormat == targetFormat) {
			return srcColor;
		}
	}
	else if (IS_ARRAY(args[0])) {
		srcPixelsArray = GET_ARG(0, GetArray);
		if (!srcPixelsArray) {
			return NULL_VAL;
		}

		srcFormat = GET_ARG(1, GetInteger);

		destPixelsArray = GET_ARG(2, GetArray);
		if (!destPixelsArray) {
			return NULL_VAL;
		}

		targetFormat = GET_ARG(3, GetInteger);
		paletteArray = GET_ARG_OPT(4, GetArray, nullptr);

		VALIDATE_TEXTURE_FORMAT(srcFormat);
		VALIDATE_TEXTURE_FORMAT(targetFormat);
	}
	else {
		srcTexture = GET_ARG(0, GetDrawable);
		if (srcTexture == nullptr) {
			return NULL_VAL;
		}

		srcFormat = srcTexture->Format;

		if (srcTexture->Access == TextureAccess_RENDERTARGET) {
			throw ScriptException(
				"Cannot directly get a pixel of a draw target texture!");
		}

		destPixelsArray = GET_ARG(1, GetArray);
		if (!destPixelsArray) {
			return NULL_VAL;
		}

		targetFormat = GET_ARG(2, GetInteger);
		paletteArray = GET_ARG_OPT(3, GetArray, nullptr);

		VALIDATE_TEXTURE_FORMAT(targetFormat);
	}

	void* destPixels = nullptr;
	size_t numPixels = 0;

	size_t targetBpp = Texture::GetFormatBytesPerPixel(targetFormat);

	Uint32* palette = nullptr;
	Uint32* paletteFromArray = nullptr;
	unsigned numPaletteColors = 0;
	int transparentIndex = 0;

	// Determine which palette to use
	if (paletteArray) {
		paletteFromArray = GetPaletteFromArray(paletteArray, numPaletteColors);
		palette = paletteFromArray;
	}
	else if (srcTexture) {
		palette = srcTexture->PaletteColors;
		numPaletteColors = srcTexture->NumPaletteColors;
	}

	if (palette) {
		transparentIndex = Graphics::GetPaletteTransparentColor(palette, numPaletteColors);
		if (transparentIndex == -1) {
			transparentIndex = 0;
		}
	}
	else if (srcFormat != targetFormat &&
		(srcFormat == TextureFormat_INDEXED || targetFormat == TextureFormat_INDEXED)) {
		throw ScriptException("Must specify palette!");
	}

	if (!IS_NULL(srcColor)) {
		int color = AS_INTEGER(srcColor);
		int destColor = 0;

#if HATCH_LITTLE_ENDIAN
		CONVERT_TEXTURE_PIXEL(color, srcFormat);
#endif

		if (srcFormat == TextureFormat_INDEXED) {
			Texture::ConvertPixelsToNonIndexed(
				&destColor, &color, 1, targetFormat, palette, numPaletteColors);
		}
		else if (targetFormat == TextureFormat_INDEXED) {
			Texture::ConvertPixelsToIndexed(&destColor,
				&color,
				srcFormat,
				1,
				palette,
				numPaletteColors,
				transparentIndex);
		}
		else {
			Texture::ConvertPixel(
				(Uint8*)&color, srcFormat, (Uint8*)&destColor, targetFormat);
		}

		Memory::Free(paletteFromArray);

#if HATCH_LITTLE_ENDIAN
		CONVERT_TEXTURE_PIXEL(destColor, targetFormat);
#endif

		return INTEGER_VAL(destColor);
	}
	else if (srcPixelsArray) {
		Uint8* pixels = nullptr;

		if (ScriptManager::Lock()) {
			try {
				pixels = GetPixelDataFromArray(srcPixelsArray, srcFormat);
			} catch (const ScriptException& error) {
				Memory::Free(paletteFromArray);
				ScriptManager::Unlock();
				throw error;
			}
			ScriptManager::Unlock();
		}

		numPixels = srcPixelsArray->Values->size();

		if (srcFormat != targetFormat) {
			if (targetFormat == TextureFormat_INDEXED) {
				destPixels = Texture::GetPalettizedPixels(pixels,
					srcFormat,
					numPixels,
					palette,
					numPaletteColors,
					transparentIndex);
			}
			else {
				destPixels = Texture::GetNonIndexedPixels(
					pixels, numPixels, targetFormat, palette, numPaletteColors);
			}

			Memory::Free(pixels);

			if (!destPixels) {
				Memory::Free(paletteFromArray);
				throw ScriptException("Could not convert pixels!");
			}
		}
		else {
			destPixels = pixels;
		}
	}
	else {
		numPixels = srcTexture->Width * srcTexture->Height;
		destPixels = Memory::Malloc(numPixels * targetBpp);
		if (!destPixels) {
			Memory::Free(paletteFromArray);
			throw ScriptException("Could not convert pixels!");
		}

		if (srcFormat == targetFormat) {
			memcpy(destPixels, srcTexture->Pixels, numPixels * targetBpp);
		}
		else if (srcFormat == TextureFormat_INDEXED) {
			Texture::ConvertPixelsToNonIndexed(destPixels,
				srcTexture->Pixels,
				numPixels,
				targetFormat,
				palette,
				numPaletteColors);
		}
		else if (targetFormat == TextureFormat_INDEXED) {
			Texture::ConvertPixelsToIndexed(destPixels,
				srcTexture->Pixels,
				srcFormat,
				numPixels,
				palette,
				numPaletteColors,
				transparentIndex);
		}
		else {
			Texture::Convert(srcTexture->Pixels,
				srcTexture->Format,
				srcTexture->Pitch,
				0,
				0,
				(Uint8*)destPixels,
				targetFormat,
				srcTexture->Width * targetBpp,
				0,
				0,
				srcTexture->Width,
				srcTexture->Height);
		}
	}

	destPixelsArray->Values->clear();

	for (size_t i = 0; i < numPixels; i++) {
		int pixel = Texture::GetPixel(destPixels, i, targetBpp);

#if HATCH_LITTLE_ENDIAN
		CONVERT_TEXTURE_PIXEL(pixel, targetFormat);
#endif

		destPixelsArray->Values->push_back(INTEGER_VAL(pixel));
	}

	Memory::Free(destPixels);
	Memory::Free(paletteFromArray);

	return NULL_VAL;
}
#undef CONVERT_TEXTURE_PIXEL
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
/***
 * Texture.CanConvertBetweenFormats
 * \desc Returns whether <param srcFormat> can be converted into <param targetFormat>.
 * \param srcFormat (<ref TEXTUREFORMAT_*>): The source format.
 * \param targetFormat (<ref TEXTUREFORMAT_*>): The target format.
 * \return boolean Returns a boolean value.
 * \ns Texture
 */
VMValue TextureImpl::VM_CanConvertBetweenFormats(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	int srcFormat = GET_ARG(0, GetInteger);
	int targetFormat = GET_ARG(1, GetInteger);

	VALIDATE_TEXTURE_FORMAT(srcFormat);
	VALIDATE_TEXTURE_FORMAT(targetFormat);

	return INTEGER_VAL(Texture::CanConvertBetweenFormats(srcFormat, targetFormat));
}
/***
 * Texture.FormatHasAlphaChannel
 * \desc Returns whether <param format> has an alpha channel.
 * \param format (<ref TEXTUREFORMAT_*>): The texture format to check.
 * \return boolean Returns a boolean value.
 * \ns Texture
 */
VMValue TextureImpl::VM_FormatHasAlphaChannel(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	int format = GET_ARG(0, GetInteger);

	VALIDATE_TEXTURE_FORMAT(format);

	return INTEGER_VAL(Texture::FormatHasAlphaChannel(format));
}

#undef VALIDATE_TEXTURE_FORMAT
#undef CHECK_EXISTS
#undef CHECK_NOT_TRYING_TO_UPDATE
#undef GET_ARG
#undef GET_ARG_OPT
