#include <Engine/Diagnostics/Memory.h>
#include <Engine/Includes/ChainedHashMap.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Utilities/ColorUtils.h>

Texture* Texture::New(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
	Texture* texture = (Texture*)Memory::TrackedCalloc("Texture::Texture", 1, sizeof(Texture));
	texture->Format = format;
	texture->Access = access;
	texture->Width = width;
	texture->Height = height;
	texture->Pixels = Memory::TrackedCalloc(
		"Texture::Pixels", 1, sizeof(Uint32) * texture->Width * texture->Height);
	return texture;
}

void Texture::SetPalette(Uint32* palette, unsigned numPaletteColors) {
	Memory::Free(PaletteColors);

	if (palette && numPaletteColors) {
		Paletted = true;
		PaletteColors = palette;
		NumPaletteColors = numPaletteColors;
	}
	else {
		Paletted = false;
		PaletteColors = nullptr;
		NumPaletteColors = 0;
	}
}

bool Texture::ConvertToRGBA() {
	if (!Paletted) {
		return false;
	}

	Uint32* pixels = (Uint32*)Pixels;

	for (size_t i = 0; i < Width * Height; i++) {
		if (pixels[i]) {
			pixels[i] = PaletteColors[pixels[i]];
		}
	}

	SetPalette(nullptr, 0);

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

	Paletted = true;

	return true;
}

void Texture::Copy(Texture* source) {
	if (!Pixels || !source || !source->Pixels) {
		return;
	}

	Uint32* dest = (Uint32*)Pixels;
	Uint32* src = (Uint32*)source->Pixels;
	size_t stride = 4;

	for (unsigned y = 0; y < Height; y++) {
		size_t sz = source->Width;
		if (sz > Width) {
			sz = Width;
		}
		memcpy(&dest[y * (Width * stride)],
			&src[y * (source->Width * stride)],
			sz * stride);
	}
}

void Texture::Dispose() {
	Memory::Free(PaletteColors);
	Memory::Free(Pixels);

	PaletteColors = nullptr;
	Pixels = nullptr;
}
