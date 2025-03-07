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
	int Pitch;
	Uint32 ID;
	void* DriverData;
	Texture* Prev;
	Texture* Next;
	bool Paletted;
	Uint32* PaletteColors;
	unsigned NumPaletteColors;

	static Texture* New(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	void SetPalette(Uint32* palette, unsigned numPaletteColors);
	bool ConvertToRGBA();
	bool ConvertToPalette(Uint32* palColors, unsigned numPaletteColors);
	void Copy(Texture* source);
	void Dispose();
};

#endif /* ENGINE_RENDERING_TEXTURE_H */
