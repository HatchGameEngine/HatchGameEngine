#include <Engine/Diagnostics/Memory.h>
#include <Engine/ResourceTypes/ImageFormats/ImageFormat.h>

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
