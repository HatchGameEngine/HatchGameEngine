#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Graphics.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Math/FixedPoint.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Utilities/ColorUtils.h>
#include <Libraries/ankerl/unordered_dense.h>

Texture* Texture::New(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
	Texture* texture = (Texture*)Memory::TrackedCalloc("Texture::Texture", 1, sizeof(Texture));
	Texture::Initialize(texture, format, access, width, height);
	return texture;
}
bool Texture::Initialize(Texture* texture,
	Uint32 format,
	Uint32 access,
	Uint32 width,
	Uint32 height) {
	if (texture->Pixels) {
		Memory::Free(texture->Pixels);
	}

	if (format == TextureFormat_NATIVE) {
		format = Graphics::TextureFormat;
	}

	texture->Format = format;
	texture->DriverFormat = format;
	texture->Access = access;
	texture->Width = width;
	texture->Height = height;
	texture->BytesPerPixel = Texture::GetFormatBytesPerPixel(format);
	texture->Pitch = width * texture->BytesPerPixel;
	texture->Pixels = Memory::TrackedCalloc("Texture::Pixels",
		texture->Width * texture->Height,
		GetFormatBytesPerPixel(format));

	return texture->Pixels != nullptr;
}
bool Texture::Reinitialize(Texture* texture,
	Uint32 format,
	Uint32 access,
	Uint32 width,
	Uint32 height) {
	// Free palette if no longer indexed
	if (format != TextureFormat_INDEXED && texture->Format != format) {
		Memory::Free(texture->PaletteColors);
		texture->PaletteColors = nullptr;
		texture->NumPaletteColors = 0;
	}

	return Texture::Initialize(texture, format, access, width, height);
}

void Texture::SetPalette(Uint32* palette, unsigned numPaletteColors) {
	Memory::Free(PaletteColors);

	if (palette && numPaletteColors) {
		PaletteColors = palette;
		NumPaletteColors = numPaletteColors;
	}
	else {
		PaletteColors = nullptr;
		NumPaletteColors = 0;
	}
}

int Texture::GetFormatBytesPerPixel(int textureFormat) {
	switch (textureFormat) {
	case TextureFormat_ARGB8888:
	case TextureFormat_ABGR8888:
	case TextureFormat_RGBA8888:
		return 4;
	case TextureFormat_RGB888:
	case TextureFormat_BGR888:
		return 3;
	case TextureFormat_INDEXED:
		return 1;
	case TextureFormat_YV12:
	case TextureFormat_IYUV:
	case TextureFormat_YUY2:
	case TextureFormat_UYVY:
	case TextureFormat_YVYU:
	case TextureFormat_NV12:
	case TextureFormat_NV21:
		return 1;
	default:
		return 4;
	}
}
int Texture::PixelFormatToTextureFormat(int pixelFormat) {
	switch (pixelFormat) {
	case PixelFormat_RGBA8888:
		return TextureFormat_RGBA8888;
	case PixelFormat_ABGR8888:
		return TextureFormat_ABGR8888;
	case PixelFormat_ARGB8888:
		return TextureFormat_ARGB8888;
	case PixelFormat_RGB888:
		return TextureFormat_RGB888;
	case PixelFormat_BGR888:
		return TextureFormat_BGR888;
	default:
		return TextureFormat_RGBA8888;
	}
}
int Texture::TextureFormatToPixelFormat(int textureFormat) {
	switch (textureFormat) {
	case TextureFormat_RGBA8888:
		return PixelFormat_RGBA8888;
	case TextureFormat_ABGR8888:
		return PixelFormat_ABGR8888;
	case TextureFormat_ARGB8888:
		return PixelFormat_ARGB8888;
	case TextureFormat_RGB888:
		return PixelFormat_RGB888;
	case TextureFormat_BGR888:
		return PixelFormat_BGR888;
	default:
		return PixelFormat_RGBA8888;
	}
}
int Texture::FormatWithAlphaChannel(int textureFormat) {
	switch (textureFormat) {
	case TextureFormat_RGB888:
		return PixelFormat_RGBA8888;
	case TextureFormat_BGR888:
		return PixelFormat_ABGR8888;
	default:
		return PixelFormat_RGBA8888;
	}
}
int Texture::FormatWithoutAlphaChannel(int textureFormat) {
	switch (textureFormat) {
	case TextureFormat_RGBA8888:
	case TextureFormat_ARGB8888:
		return PixelFormat_RGB888;
	case TextureFormat_ABGR8888:
		return PixelFormat_BGR888;
	default:
		return PixelFormat_RGB888;
	}
}
bool Texture::AreFormatsCompatible(int sourceFormat, int destFormat) {
	if (sourceFormat == destFormat) {
		return true;
	}

	if (TEXTUREFORMAT_IS_RGBA(sourceFormat) || TEXTUREFORMAT_IS_RGB(sourceFormat)) {
		return TEXTUREFORMAT_IS_RGBA(destFormat) || TEXTUREFORMAT_IS_RGB(destFormat);
	}

	return false;
}
bool Texture::CanConvertBetweenFormats(int sourceFormat, int destFormat) {
	if (AreFormatsCompatible(sourceFormat, destFormat)) {
		return true;
	}

	// It's permitted to convert an indexed texture to RGBA.
	// The OpenGL renderer, for example, requires indexed textures to be stored as RGBA.
	if (TEXTUREFORMAT_IS_RGBA(sourceFormat) || TEXTUREFORMAT_IS_RGB(sourceFormat) ||
		sourceFormat == TextureFormat_INDEXED) {
		return TEXTUREFORMAT_IS_RGBA(destFormat) || TEXTUREFORMAT_IS_RGB(destFormat) ||
			destFormat == TextureFormat_INDEXED;
	}

	return false;
}

bool Texture::KeepDriverPixelsResident() {
	if (Access != TextureAccess_STATIC || Format == TextureFormat_INDEXED) {
		return true;
	}

	return Format != DriverFormat;
}

Uint8* Texture::GetPalettizedPixels(Uint32* palColors,
	unsigned numPaletteColors,
	unsigned transparentIndex) {
	if (!palColors || Format == TextureFormat_INDEXED) {
		return nullptr;
	}

	Uint8* srcPixels = (Uint8*)Pixels;
	Uint8* destPixels = (Uint8*)Memory::Malloc(Width * Height);
	if (!destPixels) {
		return nullptr;
	}

	ankerl::unordered_dense::map<Uint32, Uint8> colorsMap;

	for (size_t i = 0; i < Width * Height; i++) {
		Uint8 rgba[4];
		ConvertPixel(srcPixels, Format, rgba, TextureFormat_RGBA8888);

		if (rgba[3]) {
			// The order doesn't matter; this is just being used as a hash
			Uint32 color = rgba[0] | rgba[1] << 8 | rgba[2] << 16 | rgba[3] << 24;

			if (colorsMap.count(color) > 0) {
				destPixels[i] = colorsMap[color];
			}
			else {
				Uint8 nearestColor = ColorUtils::NearestColor(
					rgba[0], rgba[1], rgba[2], palColors, numPaletteColors);
				destPixels[i] = nearestColor;
				colorsMap[color] = nearestColor;
			}
		}
		else {
			destPixels[i] = transparentIndex;
		}

		srcPixels += BytesPerPixel;
	}

	return destPixels;
}

void* Texture::GetNonIndexedPixels(int destFormat, Uint32* palColors) {
	if (!palColors || Format != TextureFormat_INDEXED) {
		return nullptr;
	}

	int srcPixelFormat = PixelFormatToTextureFormat(Graphics::PreferredPixelFormat);
	int destPixelFormat = TextureFormatToPixelFormat(destFormat);
	int destBytesPerPixel = GetFormatBytesPerPixel(destFormat);

	Uint8* srcPixels = (Uint8*)Pixels;
	void* destPixels = Memory::Malloc(Width * Height * destBytesPerPixel);
	if (!destPixels) {
		return nullptr;
	}

	Uint8* destPtr = (Uint8*)destPixels;

	for (size_t i = 0; i < Width * Height; i++) {
		Uint32 color = palColors[srcPixels[i]];

		color = ColorUtils::Convert(color, Graphics::PreferredPixelFormat, destPixelFormat);

		ConvertPixel((Uint8*)&color, srcPixelFormat, destPtr, destFormat);

		destPtr += destBytesPerPixel;
	}

	return destPixels;
}

void Texture::Convert(void* srcPixels,
	int srcFormat,
	int srcPitch,
	int srcX,
	int srcY,
	void* destPixels,
	int destFormat,
	int destPitch,
	int destX,
	int destY,
	int width,
	int height) {
	if (!CanConvertBetweenFormats(srcFormat, destFormat)) {
		return;
	}

	int srcBytesPerPixel = GetFormatBytesPerPixel(srcFormat);
	int destBytesPerPixel = GetFormatBytesPerPixel(destFormat);

	Uint8* src = (Uint8*)srcPixels;
	Uint8* dest = (Uint8*)destPixels;

	if (srcFormat == destFormat) {
		for (int y = 0; y < height; y++) {
			size_t srcPos = ((srcY + y) * srcPitch) + (srcX * srcBytesPerPixel);
			size_t destPos = ((destY + y) * destPitch) + (destX * destBytesPerPixel);

			memmove(&dest[destPos], &src[srcPos], (size_t)width * destBytesPerPixel);
		}
		return;
	}

	for (int y = 0; y < height; y++) {
		size_t srcPos = ((srcY + y) * srcPitch) + (srcX * srcBytesPerPixel);
		size_t destPos = ((destY + y) * destPitch) + (destX * destBytesPerPixel);

		for (int x = 0; x < width; x++) {
			Uint8* srcPtr = src + srcPos;
			Uint8* destPtr = dest + destPos;

			ConvertPixel(srcPtr, srcFormat, destPtr, destFormat);

			srcPos += srcBytesPerPixel;
			destPos += destBytesPerPixel;
		}
	}
}

void Texture::ConvertPixel(Uint8* srcPtr, int srcFormat, Uint8* destPtr, int destFormat) {
	Uint8 red = 0;
	Uint8 green = 0;
	Uint8 blue = 0;
	Uint8 alpha = 0;

	switch (srcFormat) {
	case TextureFormat_RGBA8888:
		red = srcPtr[0];
		green = srcPtr[1];
		blue = srcPtr[2];
		alpha = srcPtr[3];
		break;
	case TextureFormat_ARGB8888:
		alpha = srcPtr[0];
		red = srcPtr[1];
		green = srcPtr[2];
		blue = srcPtr[3];
		break;
	case TextureFormat_ABGR8888:
		alpha = srcPtr[0];
		blue = srcPtr[1];
		green = srcPtr[2];
		red = srcPtr[3];
		break;
	case TextureFormat_RGB888:
		red = srcPtr[0];
		green = srcPtr[1];
		blue = srcPtr[2];
		alpha = 0xFF;
		break;
	case TextureFormat_BGR888:
		blue = srcPtr[0];
		green = srcPtr[1];
		red = srcPtr[2];
		alpha = 0xFF;
		break;
	case TextureFormat_INDEXED:
		red = srcPtr[0];
		break;
	}

	switch (destFormat) {
	case TextureFormat_RGBA8888:
		destPtr[0] = red;
		destPtr[1] = green;
		destPtr[2] = blue;
		destPtr[3] = alpha;
		break;
	case TextureFormat_ARGB8888:
		destPtr[0] = alpha;
		destPtr[1] = red;
		destPtr[2] = green;
		destPtr[3] = blue;
		break;
	case TextureFormat_ABGR8888:
		destPtr[0] = alpha;
		destPtr[1] = blue;
		destPtr[2] = green;
		destPtr[3] = red;
		break;
	case TextureFormat_RGB888:
		destPtr[0] = red;
		destPtr[1] = green;
		destPtr[2] = blue;
		break;
	case TextureFormat_BGR888:
		destPtr[0] = blue;
		destPtr[1] = green;
		destPtr[2] = red;
		break;
	case TextureFormat_INDEXED:
		destPtr[0] = red;
		break;
	}
}

void Texture::CopyPixels(Texture* srcTexture,
	int srcX,
	int srcY,
	int srcWidth,
	int srcHeight,
	int destX,
	int destY) {
	if (!Pixels || !srcTexture || !srcTexture->Pixels) {
		return;
	}

	int destWidth, destHeight;

	if (!Texture::ClipCopyRegion(srcTexture->Width,
		    srcTexture->Height,
		    srcX,
		    srcY,
		    srcWidth,
		    srcHeight,
		    Width,
		    Height,
		    destX,
		    destY,
		    destWidth,
		    destHeight)) {
		return;
	}

	Texture::Convert((Uint8*)srcTexture->Pixels,
		srcTexture->Format,
		srcTexture->Pitch,
		srcX,
		srcY,
		(Uint8*)Pixels,
		Format,
		Pitch,
		destX,
		destY,
		destWidth,
		destHeight);
}

void Texture::CopyPixels(void* srcPixels,
	int srcFormat,
	int srcX,
	int srcY,
	int srcWidth,
	int srcHeight,
	int destX,
	int destY) {
	if (!Pixels || !srcPixels) {
		return;
	}

	int destWidth, destHeight;

	if (!Texture::ClipCopyRegion(srcWidth,
		    srcHeight,
		    srcX,
		    srcY,
		    srcWidth,
		    srcHeight,
		    Width,
		    Height,
		    destX,
		    destY,
		    destWidth,
		    destHeight)) {
		return;
	}

	Texture::Convert((Uint8*)srcPixels,
		srcFormat,
		srcWidth * GetFormatBytesPerPixel(srcFormat),
		srcX,
		srcY,
		(Uint8*)Pixels,
		Format,
		Pitch,
		destX,
		destY,
		destWidth,
		destHeight);
}

bool Texture::ClipCopyRegion(int srcTextureWidth,
	int srcTextureHeight,
	int& srcX,
	int& srcY,
	int& srcWidth,
	int& srcHeight,
	int destTextureWidth,
	int destTextureHeight,
	int& destX,
	int& destY,
	int& destWidth,
	int& destHeight) {
	if (srcX < 0) {
		srcWidth += srcX;
		if (srcWidth <= 0) {
			return false;
		}
		srcX = 0;
	}
	if (srcY < 0) {
		srcHeight += srcY;
		if (srcHeight <= 0) {
			return false;
		}
		srcY = 0;
	}
	if (srcX >= srcTextureWidth || srcY >= srcTextureHeight) {
		return false;
	}

	if (srcWidth > srcTextureWidth - srcX) {
		srcWidth = srcTextureWidth - srcX;
	}
	if (srcHeight > srcTextureHeight - srcY) {
		srcHeight = srcTextureHeight - srcY;
	}

	destWidth = srcWidth;
	destHeight = srcHeight;

	if (destX < 0) {
		destWidth += destX;
		if (destWidth <= 0) {
			return false;
		}
		destX = 0;
	}
	if (destY < 0) {
		destHeight += destY;
		if (destHeight <= 0) {
			return false;
		}
		destY = 0;
	}
	if (destX >= destTextureWidth || destY >= destTextureHeight) {
		return false;
	}

	if (destWidth > destTextureWidth - destX) {
		destWidth = destTextureWidth - destX;
	}
	if (destHeight > destTextureHeight - destY) {
		destHeight = destTextureHeight - destY;
	}
	if (destWidth <= 0 || destHeight <= 0) {
		return false;
	}

	return true;
}

void* Texture::Crop(Texture* source, int cropX, int cropY, int cropWidth, int cropHeight) {
	if (cropX < 0 || cropY < 0 || cropWidth <= 0 || cropHeight <= 0) {
		return nullptr;
	}

	int endX = std::min(cropX + cropWidth, (int)source->Width);
	int endY = std::min(cropY + cropHeight, (int)source->Height);

	int copyWidth = endX - cropX;
	int copyHeight = endY - cropY;

	int bpp = source->BytesPerPixel;

	Uint8* pixels = (Uint8*)Memory::Calloc(cropWidth * cropHeight, bpp);

	if (copyWidth > 0 && copyHeight > 0) {
		Uint8* src = (Uint8*)source->Pixels + (cropY * source->Pitch);
		Uint8* dest = pixels;

		for (int srcY = 0; srcY < copyHeight; srcY++) {
			memcpy(dest, src + (cropX * bpp), copyWidth * bpp);

			src += source->Pitch;
			dest += cropWidth * bpp;
		}
	}

	return pixels;
}

void Texture::ScaleIntoBuffer(Texture* source,
	int srcX,
	int srcY,
	int srcWidth,
	int srcHeight,
	void* destPixels,
	int destWidth,
	int destHeight,
	int destFormat) {
	int srcBytesPerPixel = source->BytesPerPixel;
	int destBytesPerPixel = GetFormatBytesPerPixel(destFormat);

	Uint32 xStep, yStep;

	Uint32 maxWidth = std::min((Uint32)srcWidth, source->Width) << 16;
	Uint32 maxHeight = std::min((Uint32)srcHeight, source->Height) << 16;

	if (maxWidth == 0 || maxHeight == 0) {
		return;
	}

	xStep = FP16_DIVIDE(FP16_TO(1.0f), FP16_DIVIDE(destWidth << 16, maxWidth));
	yStep = FP16_DIVIDE(FP16_TO(1.0f), FP16_DIVIDE(destHeight << 16, maxHeight));

	srcX <<= 16;
	srcY <<= 16;

	Uint8* dest = (Uint8*)destPixels;

	for (Uint32 sy = srcY; sy < srcY + maxHeight; sy += yStep) {
		Uint8* row = (Uint8*)source->Pixels + ((sy >> 16) * source->Pitch);

		for (Uint32 sx = srcX; sx < srcX + maxWidth; sx += xStep) {
			ConvertPixel(row + ((sx >> 16) * srcBytesPerPixel),
				source->Format,
				dest,
				destFormat);

			dest += destBytesPerPixel;
		}
	}
}

void Texture::ScaleInto(Texture* source,
	int srcX,
	int srcY,
	int srcWidth,
	int srcHeight,
	Texture* dest) {
	Texture::ScaleIntoBuffer(source,
		srcX,
		srcY,
		srcWidth,
		srcHeight,
		dest->Pixels,
		dest->Width,
		dest->Height,
		dest->Format);
}

void* Texture::GetScaledPixels(Texture* source,
	int srcX,
	int srcY,
	int srcWidth,
	int srcHeight,
	int destWidth,
	int destHeight,
	int destFormat) {
	int destBytesPerPixel = GetFormatBytesPerPixel(destFormat);
	void* pixels = Memory::Malloc(destWidth * destHeight * destBytesPerPixel);
	if (pixels) {
		Texture::ScaleIntoBuffer(source,
			srcX,
			srcY,
			srcWidth,
			srcHeight,
			pixels,
			destWidth,
			destHeight,
			destFormat);
	}
	return pixels;
}

int Texture::GetPixel(int x, int y) {
	Uint8* pixels = (Uint8*)Pixels;

	pixels += y * Pitch;
	pixels += x * BytesPerPixel;

	switch (BytesPerPixel) {
	case 4:
		return pixels[0] | (pixels[1] << 8) | (pixels[2] << 16) | (pixels[3] << 24);
	case 3:
		return pixels[0] | (pixels[1] << 8) | (pixels[2] << 16);
	case 2:
		return pixels[0] | (pixels[1] << 8);
	case 1:
		return pixels[0];
	}

	return 0;
}

void Texture::SetPixel(int x, int y, int color) {
	Uint8* pixels = (Uint8*)Pixels;

	pixels += y * Pitch;
	pixels += x * BytesPerPixel;

	if (Format == TextureFormat_INDEXED) {
		*pixels = color;
	}
	else if (BytesPerPixel == 4) {
		pixels[0] = (color >> 24) & 0xFF;
		pixels[1] = (color >> 16) & 0xFF;
		pixels[2] = (color >> 8) & 0xFF;
		pixels[3] = color & 0xFF;
	}
	else {
		pixels[0] = (color >> 16) & 0xFF;
		pixels[1] = (color >> 8) & 0xFF;
		pixels[2] = color & 0xFF;
	}
}

void Texture::Dispose() {
	Memory::Free(PaletteColors);
	Memory::Free(Pixels);
	Memory::Free(DriverPixelData);

	ScriptManager::RegistryRemove(this);

	PaletteColors = nullptr;
	Pixels = nullptr;
}
