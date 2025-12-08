#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Includes/ChainedHashMap.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Utilities/ColorUtils.h>

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
	if (texture->PaletteColors) {
		Memory::Free(texture->PaletteColors);
	}

	texture->Format = format;
	texture->DriverFormat = format;
	texture->Access = access;
	texture->Width = width;
	texture->Height = height;
	texture->Pixels = Memory::TrackedCalloc(
		"Texture::Pixels", 1, sizeof(Uint32) * texture->Width * texture->Height);
	texture->PaletteColors = nullptr;
	texture->NumPaletteColors = 0;

	return texture->Pixels != nullptr;
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
	case TextureFormat_INDEXED:
		// "Why four bytes? Aren't indexed formats typically 1 byte per pixel?"
		// Yes, but indexed textures are stored in memory as if they were ABGR.
		// This also simplifies the process of sending those textures to the GPU,
		// since modern GPUs don't support indexed textures.
		// If this is optimized in the future, and the conversion to ABGR happens
		// at texture upload time, this can be changed to 1.
		return 4;
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
	case PixelFormat_ARGB8888:
		return TextureFormat_ARGB8888;
	case PixelFormat_ABGR8888:
		return TextureFormat_ABGR8888;
	default:
		return TextureFormat_RGBA8888;
	}
}
int Texture::TextureFormatToPixelFormat(int textureFormat) {
	switch (textureFormat) {
	case TextureFormat_ARGB8888:
		return PixelFormat_ARGB8888;
	case TextureFormat_ABGR8888:
		return PixelFormat_ABGR8888;
	default:
		return PixelFormat_ARGB8888;
	}
}
bool Texture::CanConvertBetweenFormats(int sourceFormat, int destFormat) {
	if (sourceFormat == destFormat) {
		return true;
	}

	if (TEXTUREFORMAT_IS_RGBA(sourceFormat)) {
		return TEXTUREFORMAT_IS_RGBA(destFormat);
	}

	return false;
}

bool Texture::ConvertToRGBA() {
	if (Format != TextureFormat_INDEXED) {
		return false;
	}

	Uint32* pixels = (Uint32*)Pixels;

	for (size_t i = 0; i < Width * Height; i++) {
		if (pixels[i]) {
			pixels[i] = PaletteColors[pixels[i]];
		}
	}

	SetPalette(nullptr, 0);

	Format = TextureFormat_ARGB8888;

	return true;
}
bool Texture::ConvertToPalette(Uint32* palColors, unsigned numPaletteColors) {
	ConvertToRGBA();

	Uint32* pixels = (Uint32*)Pixels;
	int nearestColor;

	ChainedHashMap<int>* colorsHash = new ChainedHashMap<int>(NULL, 256);

	for (size_t i = 0; i < Width * Height; i++) {
		Uint32 color = pixels[i];
		if (color & 0xFF000000) {
			if (!colorsHash->GetIfExists(color, &nearestColor)) {
				Uint8 rgb[3];
				ColorUtils::SeparateRGB(color, rgb);
				nearestColor = ColorUtils::NearestColor(
					rgb[0], rgb[1], rgb[2], palColors, numPaletteColors);
				colorsHash->Put(color, nearestColor);
			}

			pixels[i] = nearestColor;
		}
		else {
			pixels[i] = 0;
		}
	}

	delete colorsHash;

	Format = TextureFormat_INDEXED;

	return true;
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
			Uint32 val = TO_LE32(*(Uint32*)(&src[srcPos]));

			Uint8 red = 0;
			Uint8 green = 0;
			Uint8 blue = 0;
			Uint8 alpha = 0;

			switch (srcFormat) {
			case TextureFormat_ARGB8888:
				alpha = (val >> 24) & 0xFF;
				red = (val >> 16) & 0xFF;
				green = (val >> 8) & 0xFF;
				blue = val & 0xFF;
				break;
			case TextureFormat_ABGR8888:
				alpha = (val >> 24) & 0xFF;
				blue = (val >> 16) & 0xFF;
				green = (val >> 8) & 0xFF;
				red = val & 0xFF;
				break;
			case TextureFormat_RGBA8888:
				red = (val >> 24) & 0xFF;
				green = (val >> 16) & 0xFF;
				blue = (val >> 8) & 0xFF;
				alpha = val & 0xFF;
				break;
			}

			Uint32* destPtr = (Uint32*)(&dest[destPos]);

			switch (destFormat) {
			case TextureFormat_RGBA8888:
				val = red << 24;
				val |= green << 16;
				val |= blue << 8;
				val |= alpha;
				break;
			case TextureFormat_ABGR8888:
				val = alpha << 24;
				val |= blue << 16;
				val |= green << 8;
				val |= red;
				break;
			case TextureFormat_ARGB8888:
				val = alpha << 24;
				val |= red << 16;
				val |= green << 8;
				val |= blue;
				break;
			}

			*destPtr = TO_LE32(val);

			srcPos += srcBytesPerPixel;
			destPos += destBytesPerPixel;
		}
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

void Texture::Dispose() {
	Memory::Free(PaletteColors);
	Memory::Free(Pixels);
	Memory::Free(DriverPixelData);

	ScriptManager::RegistryRemove(this);

	PaletteColors = nullptr;
	Pixels = nullptr;
}
