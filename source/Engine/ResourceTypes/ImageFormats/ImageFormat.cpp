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

void ImageFormat::ReadPixelDataARGB(Uint8* pixelData, int numChannels) {
	Uint32 Rmask, Gmask, Bmask, Amask;
	bool doConvert = false;

	if (Graphics::PreferredPixelFormat == SDL_PIXELFORMAT_ABGR8888) {
		Amask = (numChannels == 4) ? 0xFF000000 : 0;
		Bmask = 0x00FF0000;
		Gmask = 0x0000FF00;
		Rmask = 0x000000FF;
		if (numChannels != 4) {
			doConvert = true;
		}
	}
	else if (Graphics::PreferredPixelFormat == SDL_PIXELFORMAT_ARGB8888) {
		Amask = (numChannels == 4) ? 0xFF000000 : 0;
		Rmask = 0x00FF0000;
		Gmask = 0x0000FF00;
		Bmask = 0x000000FF;
		doConvert = true;
	}

	if (doConvert) {
		Uint32 a, b, g, r;
		int pc = 0, size = Width * Height;
		if (Amask) {
			for (Uint8* px = pixelData; pc < size; px += 4, pc++) {
				r = px[0] | px[0] << 8 | px[0] << 16 | px[0] << 24;
				g = px[1] | px[1] << 8 | px[1] << 16 | px[1] << 24;
				b = px[2] | px[2] << 8 | px[2] << 16 | px[2] << 24;
				a = px[3] | px[3] << 8 | px[3] << 16 | px[3] << 24;
				Data[pc] = (r & Rmask) | (g & Gmask) | (b & Bmask) | (a & Amask);
			}
		}
		else {
			for (Uint8* px = pixelData; pc < size; px += 3, pc++) {
				r = px[0] | px[0] << 8 | px[0] << 16 | px[0] << 24;
				g = px[1] | px[1] << 8 | px[1] << 16 | px[1] << 24;
				b = px[2] | px[2] << 8 | px[2] << 16 | px[2] << 24;
				Data[pc] = (r & Rmask) | (g & Gmask) | (b & Bmask) | 0xFF000000;
			}
		}
	}
	else {
		memcpy(Data, pixelData, Width * Height * sizeof(Uint32));
	}
}
