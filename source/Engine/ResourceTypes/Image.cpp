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

Image::Image(Texture* texturePtr) {
	AddRef();
	Filename = nullptr;
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
	else if (memcmp(magic, "\xFF\xD8\xFF\xDB", 4) == 0 ||
		memcmp(magic, "\xFF\xD8\xFF\xEE", 4) == 0) {
		return IMAGE_FORMAT_JPEG;
	}

	return IMAGE_FORMAT_UNKNOWN;
}
bool Image::IsFile(Stream* stream) {
	return DetectFormat(stream) != IMAGE_FORMAT_UNKNOWN;
}

Texture* Image::LoadTextureFromResource(const char* filename) {
	Texture* texture = NULL;
	Uint32* data = NULL;
	Uint32 width = 0;
	Uint32 height = 0;
	Uint32* paletteColors = NULL;
	unsigned numPaletteColors = 0;

	Uint8 format = IMAGE_FORMAT_UNKNOWN;
	Stream* stream = ResourceStream::New(filename);
	if (stream) {
		format = DetectFormat(stream);
		stream->Seek(0);
	}
	else {
		return NULL;
	}

	if (format == IMAGE_FORMAT_PNG) {
		Clock::Start();
		PNG* png = PNG::Load(stream);
		if (png) {
			Log::Print(Log::LOG_VERBOSE,
				"PNG load took %.3f ms (%s)",
				Clock::End(),
				filename);
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
			stream->Close();
			Log::Print(Log::LOG_ERROR, "PNG \"%s\" could not be loaded!", filename);
			return NULL;
		}
	}
	else if (format == IMAGE_FORMAT_JPEG) {
		Clock::Start();
		JPEG* jpeg = JPEG::Load(stream);
		if (jpeg) {
			Log::Print(Log::LOG_VERBOSE,
				"JPEG load took %.3f ms (%s)",
				Clock::End(),
				filename);
			width = (Uint32)jpeg->Width;
			height = (Uint32)jpeg->Height;

			data = jpeg->Data;
			Memory::Track(data, "Texture::Data");

			delete jpeg;
		}
		else {
			stream->Close();
			Log::Print(Log::LOG_ERROR, "JPEG \"%s\" could not be loaded!", filename);
			return NULL;
		}
	}
	else if (format == IMAGE_FORMAT_GIF) {
		Clock::Start();
		GIF* gif = GIF::Load(stream);
		if (gif) {
			Log::Print(Log::LOG_VERBOSE,
				"GIF load took %.3f ms (%s)",
				Clock::End(),
				filename);
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
			stream->Close();
			Log::Print(Log::LOG_ERROR, "GIF \"%s\" could not be loaded!", filename);
			return NULL;
		}
	}
	else {
		stream->Close();
		Log::Print(Log::LOG_ERROR, "Unsupported image format for file \"%s\"!", filename);
		return NULL;
	}

	stream->Close();

	bool forceSoftwareTextures = false;
	Application::Settings->GetBool("display", "forceSoftwareTextures", &forceSoftwareTextures);
	if (forceSoftwareTextures) {
		Graphics::NoInternalTextures = true;
	}

	if (!forceSoftwareTextures &&
		(width > Graphics::MaxTextureWidth || height > Graphics::MaxTextureHeight)) {
		Log::Print(Log::LOG_WARN,
			"Image file \"%s\" of size %d x %d is larger than maximum size of %d x %d!",
			filename,
			width,
			height,
			Graphics::MaxTextureWidth,
			Graphics::MaxTextureHeight);
		// return NULL;
	}

	texture = Graphics::CreateTextureFromPixels(width, height, data, width * sizeof(Uint32));

	Graphics::SetTexturePalette(texture, paletteColors, numPaletteColors);

	Graphics::NoInternalTextures = false;

	Memory::Free(data);

	return texture;
}
