#include <Engine/ResourceTypes/Image.h>

#include <Engine/Application.h>
#include <Engine/Error.h>
#include <Engine/Graphics.h>

#include <Engine/ResourceTypes/ImageFormats/GIF.h>
#include <Engine/ResourceTypes/ImageFormats/JPEG.h>
#include <Engine/ResourceTypes/ImageFormats/PNG.h>

#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/Utilities/StringUtils.h>

Image::Image(const char* filename) {
	AddRef();
	Filename = StringUtils::Duplicate(filename);
	TexturePtr = Image::LoadTextureFromResource(Filename);
}
Image::Image(Stream* stream) {
	AddRef();
	TexturePtr = Image::LoadTextureFromStream(stream, nullptr);
	stream->Close();
}

Image::Image(Texture* texturePtr) {
	AddRef();
	TexturePtr = texturePtr;
}

void Image::AddRef() {
	References++;
}

bool Image::TakeRef() {
	if (References == 0) {
		Error::Fatal("Tried to release reference of Image when it had none!");
	}

	References--;

	return References == 0;
}

void Image::Dispose() {
	if (Filename) {
		Memory::Free(Filename);
		Filename = NULL;
	}
	if (TexturePtr) {
		Graphics::DisposeTexture(TexturePtr);
		TexturePtr = NULL;
	}
}

Image::~Image() {
	Dispose();
}

Uint8 Image::DetectFormat(Stream* stream) {
	Uint8 magic[8];
	stream->ReadBytes(magic, sizeof magic);

	// PNG
	if (memcmp(magic, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0) {
		return IMAGE_FORMAT_PNG;
	}
	// GIF
	else if (memcmp(magic, "GIF87a", 6) == 0 || memcmp(magic, "GIF89a", 6) == 0) {
		return IMAGE_FORMAT_GIF;
	}
	// JPEG
	else if (memcmp(magic, "\xFF\xD8\xFF\xE0", 4) == 0 ||
		memcmp(magic, "\xFF\xD8\xFF\xDB", 4) == 0 ||
		memcmp(magic, "\xFF\xD8\xFF\xEE", 4) == 0 ||
		memcmp(magic, "\xFF\xD8\xFF\xE1", 4) == 0) {
		return IMAGE_FORMAT_JPEG;
	}

	return IMAGE_FORMAT_UNKNOWN;
}
bool Image::IsFile(Stream* stream) {
	return DetectFormat(stream) != IMAGE_FORMAT_UNKNOWN;
}

Texture* Image::LoadTextureFromResource(const char* filename) {
	Stream* stream = ResourceStream::New(filename);
	if (!stream) {
		return NULL;
	}

	Texture* texture = LoadTextureFromStream(stream, filename);

	stream->Close();

	return texture;
}
Texture* Image::LoadTextureFromStream(Stream* stream, const char* filename) {
	Uint8* data = NULL;
	Uint32 width = 0;
	Uint32 height = 0;
	Uint32* paletteColors = NULL;
	unsigned numPaletteColors = 0;

	Uint8 format = DetectFormat(stream);
	stream->Seek(0);

	if (format == IMAGE_FORMAT_PNG) {
		Clock::Start();
		PNG* png = PNG::Load(stream);
		if (png) {
			if (filename) {
				Log::Print(Log::LOG_VERBOSE,
					"PNG load took %.3f ms (%s)",
					Clock::End(),
					filename);
			}
			else {
				Log::Print(Log::LOG_VERBOSE,
					"PNG load took %.3f ms",
					Clock::End());
			}
			width = (Uint32)png->Width;
			height = (Uint32)png->Height;

			data = png->Data;
			Memory::Track(data, "Texture::Data");

			if (png->Paletted) {
				paletteColors = png->GetPalette();
				numPaletteColors = png->NumPaletteColors;
			}

			delete png;
		}
		else {
			if (filename) {
				Log::Print(Log::LOG_ERROR, "PNG \"%s\" could not be loaded!", filename);
			}
			else {
				Log::Print(Log::LOG_ERROR, "PNG could not be loaded!");
			}
			return NULL;
		}
	}
	else if (format == IMAGE_FORMAT_JPEG) {
		Clock::Start();
		JPEG* jpeg = JPEG::Load(stream);
		if (jpeg) {
			if (filename) {
				Log::Print(Log::LOG_VERBOSE,
					"JPEG load took %.3f ms (%s)",
					Clock::End(),
					filename);
			}
			else {
				Log::Print(Log::LOG_VERBOSE,
					"JPEG load took %.3f ms",
					Clock::End());
			}
			width = (Uint32)jpeg->Width;
			height = (Uint32)jpeg->Height;

			data = jpeg->Data;
			Memory::Track(data, "Texture::Data");

			delete jpeg;
		}
		else {
			if (filename) {
				Log::Print(Log::LOG_ERROR, "JPEG \"%s\" could not be loaded!", filename);
			}
			else {
				Log::Print(Log::LOG_ERROR, "JPEG could not be loaded!");
			}
			return NULL;
		}
	}
	else if (format == IMAGE_FORMAT_GIF) {
		Clock::Start();
		GIF* gif = GIF::Load(stream);
		if (gif) {
			if (filename) {
				Log::Print(Log::LOG_VERBOSE,
					"GIF load took %.3f ms (%s)",
					Clock::End(),
					filename);
			}
			else {
				Log::Print(Log::LOG_VERBOSE,
					"GIF load took %.3f ms",
					Clock::End());
			}
			width = (Uint32)gif->Width;
			height = (Uint32)gif->Height;

			data = gif->Data;
			Memory::Track(data, "Texture::Data");

			if (gif->Paletted) {
				paletteColors = gif->GetPalette();
				numPaletteColors = 256;
			}

			delete gif;
		}
		else {
			if (filename) {
				Log::Print(Log::LOG_ERROR, "GIF \"%s\" could not be loaded!", filename);
			}
			else {
				Log::Print(Log::LOG_ERROR, "GIF could not be loaded!");
			}
			return NULL;
		}
	}
	else {
		if (filename) {
			Log::Print(Log::LOG_ERROR, "Unsupported image format for file \"%s\"!", filename);
		}
		else {
			Log::Print(Log::LOG_ERROR, "Unsupported image format!", filename);
		}
		return NULL;
	}

	if (width > Graphics::MaxTextureWidth || height > Graphics::MaxTextureHeight) {
		Log::Print(Log::LOG_WARN,
			"Image file of size %d x %d is larger than maximum size of %d x %d!",
			width,
			height,
			Graphics::MaxTextureWidth,
			Graphics::MaxTextureHeight);
	}

	Uint32 textureFormat = paletteColors ? TextureFormat_INDEXED : Graphics::TextureFormat;
	unsigned bpp = Texture::GetFormatBytesPerPixel(textureFormat);
	Texture* texture = Graphics::CreateTextureFromPixels(
		textureFormat, width, height, data, width * bpp);
	Graphics::SetTexturePalette(texture, paletteColors, numPaletteColors);

	Memory::Free(data);

	return texture;
}
