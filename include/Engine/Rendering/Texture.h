#ifndef ENGINE_RENDERING_TEXTURE_H
#define ENGINE_RENDERING_TEXTURE_H
class Texture;

#include <Engine/Includes/Standard.h>

class Texture {
public:
	Uint32 Format;
	Uint32 Access;
	Uint32 Width;
	Uint32 Height;
	void* Pixels;
	int Pitch = 0;
	Uint32 ID = 0;
	void* DriverData = nullptr;
	Texture* Prev = nullptr;
	Texture* Next = nullptr;
	bool Paletted = false;
	Uint32* PaletteColors = nullptr;
	unsigned NumPaletteColors = 0;

	Texture(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	~Texture();

	void SetPalette(Uint32* palette, unsigned numPaletteColors);
	bool ConvertToRGBA();
	bool ConvertToPalette(Uint32* palColors, unsigned numPaletteColors);
	void Copy(Texture* source);
};

#endif /* ENGINE_RENDERING_TEXTURE_H */
