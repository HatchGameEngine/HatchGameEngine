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
	Uint32* PaletteColors;
	unsigned NumPaletteColors;

	static Texture* New(Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	static bool
	Initialize(Texture* texture, Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	static bool
	Reinitialize(Texture* texture, Uint32 format, Uint32 access, Uint32 width, Uint32 height);
	void SetPalette(Uint32* palette, unsigned numPaletteColors);
	static int GetFormatBytesPerPixel(int textureFormat);
	static int PixelFormatToTextureFormat(int pixelFormat);
	static int TextureFormatToPixelFormat(int textureFormat);
	static int FormatWithAlphaChannel(int textureFormat);
	static int FormatWithoutAlphaChannel(int textureFormat);
	static bool CanConvertBetweenFormats(int sourceFormat, int destFormat);
	bool KeepDriverPixelsResident();
	static void ConvertPixelsToIndexed(void* destPixels,
		void* srcPixels,
		int srcFormat,
		size_t srcLength,
		Uint32* palColors,
		unsigned numPaletteColors,
		unsigned transparentIndex);
	static Uint8* GetPalettizedPixels(void* srcPixels,
		int srcFormat,
		int srcWidth,
		int srcHeight,
		Uint32* palColors,
		unsigned numPaletteColors,
		unsigned transparentIndex);
	static Uint8* GetPalettizedPixels(void* srcPixels,
		int srcFormat,
		size_t srcLength,
		Uint32* palColors,
		unsigned numPaletteColors,
		unsigned transparentIndex);
	static void ConvertPixelsToNonIndexed(void* destPixels,
		void* srcPixels,
		size_t srcLength,
		int destFormat,
		Uint32* palColors,
		unsigned numPaletteColors);
	static void* GetNonIndexedPixels(void* srcPixels,
		size_t srcLength,
		int destFormat,
		Uint32* palColors,
		unsigned numPaletteColors);
	static void* GetNonIndexedPixels(void* srcPixels,
		int srcWidth,
		int srcHeight,
		int destFormat,
		Uint32* palColors,
		unsigned numPaletteColors);
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
	static void ConvertPixel(Uint8* srcPtr, int srcFormat, Uint8* destPtr, int destFormat);
	void CopyPixels(Texture* srcTexture,
		int srcX,
		int srcY,
		int srcWidth,
		int srcHeight,
		int destX,
		int destY);
	void CopyPixels(void* srcPixels,
		int srcFormat,
		int srcX,
		int srcY,
		int srcWidth,
		int srcHeight,
		int destX,
		int destY,
		int copyWidth,
		int copyHeight);
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
	static void* Crop(Texture* source, int cropX, int cropY, int cropWidth, int cropHeight);
	static void ScaleIntoBuffer(Texture* source,
		int srcX,
		int srcY,
		int srcWidth,
		int srcHeight,
		void* destPixels,
		int destWidth,
		int destHeight,
		int destFormat);
	static void
	ScaleInto(Texture* source, int srcX, int srcY, int srcWidth, int srcHeight, Texture* dest);
	static void* GetScaledPixels(Texture* source,
		int srcX,
		int srcY,
		int srcWidth,
		int srcHeight,
		int destWidth,
		int destHeight,
		int destFormat);
	static int GetPixel(void* src, size_t index, size_t bytesPerPixel);
	int GetPixel(int x, int y);
	void SetPixel(int x, int y, int color);
	void Dispose();
};

#endif /* ENGINE_RENDERING_TEXTURE_H */
