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
	size_t Pitch;
	size_t BytesPerPixel;
	Uint32 ID;
	Uint32 DriverFormat;
	void* DriverPixelData;
	void* DriverData;
	Texture* Prev;
	Texture* Next;
	bool Paletted;
	Uint32* PaletteColors;
	unsigned NumPaletteColors;

	static Texture* New(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	static bool Initialize(Texture* texture, Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	static bool Reinitialize(Texture* texture, Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	void SetPalette(Uint32* palette, unsigned numPaletteColors);
	static int GetFormatBytesPerPixel(int textureFormat);
	static int PixelFormatToTextureFormat(int pixelFormat);
	static int TextureFormatToPixelFormat(int textureFormat);
	static int FormatWithAlphaChannel(int textureFormat);
	static int FormatWithoutAlphaChannel(int textureFormat);
	static bool CanConvertBetweenFormats(int sourceFormat, int destFormat);
	bool KeepDriverPixelsResident();
	bool ConvertToRGBA();
	bool ConvertToPalette(Uint32* palColors, unsigned numPaletteColors);
	static void Convert(void* srcPixels,
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
		int height);
	void CopyPixels(Texture* srcTexture,
		int srcX,
		int srcY,
		int srcWidth,
		int srcHeight,
		int destX,
		int destY);
	static bool ClipCopyRegion(int srcTextureWidth,
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
		int& destHeight);
	void Dispose();
};

#endif /* ENGINE_RENDERING_TEXTURE_H */
