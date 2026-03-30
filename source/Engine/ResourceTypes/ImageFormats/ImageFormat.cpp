#include <Engine/Diagnostics/Memory.h>
#include <Engine/Graphics.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/ImageFormats/ImageFormat.h>

#define STB_IMAGE_IMPLEMENTATION
#include <Libraries/stb_image.h>

Uint32* ImageFormat::GetPalette() {
	if (!Colors) {
		return nullptr;
	}

	Uint32* colors = (Uint32*)Memory::TrackedMalloc(
		"ImageFormat::Colors", NumPaletteColors * sizeof(Uint32));
	memcpy(colors, Colors, NumPaletteColors * sizeof(Uint32));
	return colors;
}

bool ImageFormat::Save(const char* filename) {
	return false;
}
ImageFormat::~ImageFormat() {
	Memory::Free(Colors);
	Colors = nullptr;
}

void ImageFormat::ReadPixelData(Uint8* pixelData, int numChannels) {
	Uint32* dest = (Uint32*)Data;

	if (Graphics::PreferredPixelFormat == PixelFormat_RGBA8888 && numChannels == 4) {
		memcpy(dest, pixelData, Width * Height * sizeof(Uint32));
		return;
	}

	int pc = 0, size = Width * Height;
	if (numChannels == 4) {
		for (Uint32* px = (Uint32*)pixelData; pc < size; px++, pc++) {
			dest[pc] = ColorUtils::Convert(*px, PixelFormat_RGBA8888, Graphics::PreferredPixelFormat);
		}
	}
	else {
		for (Uint8* px = pixelData; pc < size; px += numChannels, pc++) {
			dest[pc] = ColorUtils::Make(px[0], px[1], px[2], 0xFF, Graphics::PreferredPixelFormat);
		}
	}
}
