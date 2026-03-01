#include <Engine/ResourceTypes/ImageFormats/JPEG.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Scene.h>

#include <Libraries/stb_image.h>

JPEG* JPEG::Load(Stream* stream) {
	JPEG* jpeg = new JPEG;
	Uint8* pixelData = NULL;
	int num_channels;
	int width, height;
	Uint8* buffer = NULL;
	size_t buffer_len = 0;

	buffer_len = stream->Length();
	buffer = (Uint8*)malloc(buffer_len);
	stream->ReadBytes(buffer, buffer_len);

	pixelData = (Uint8*)stbi_load_from_memory(
		buffer, buffer_len, &width, &height, &num_channels, STBI_rgb);
	if (!pixelData) {
		Log::Print(Log::LOG_ERROR, "stbi_load failed: %s", stbi_failure_reason());
		goto JPEG_Load_FAIL;
	}

	jpeg->Width = width;
	jpeg->Height = height;
	jpeg->Data = (Uint32*)Memory::TrackedMalloc(
		"JPEG::Data", jpeg->Width * jpeg->Height * sizeof(Uint32));

	jpeg->ReadPixelDataARGB(pixelData, num_channels);

	jpeg->Colors = nullptr;
	jpeg->Paletted = false;
	jpeg->NumPaletteColors = 0;

	goto JPEG_Load_Success;

JPEG_Load_FAIL:
	delete jpeg;
	jpeg = NULL;

JPEG_Load_Success:
	if (buffer) {
		free(buffer);
	}
	if (pixelData) {
		stbi_image_free(pixelData);
	}
	return jpeg;
}
bool JPEG::Save(JPEG* jpeg, const char* filename) {
	return jpeg->Save(filename);
}

bool JPEG::Save(const char* filename) {
	return false;
}

JPEG::~JPEG() {}
