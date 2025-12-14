#include <Engine/ResourceTypes/ImageFormats/PNG.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Graphics.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/Rendering/Software/SoftwareRenderer.h>

#ifdef USING_LIBPNG
#include "png.h"
#ifdef PNG_READ_SUPPORTED
void png_read_fn(png_structp ctx, png_bytep area, png_size_t size) {
	Stream* stream = (Stream*)png_get_io_ptr(ctx);
	stream->ReadBytes(area, size);
}
#else
#undef USING_LIBPNG
#endif
#endif

#ifndef USING_LIBPNG
#define USING_SPNG
#endif

#ifdef USING_SPNG
#define SPNG_STATIC
#ifndef _WIN32
#define SPNG_USE_SIMD
#endif
#include <Libraries/spng.h>
#else
#define STB_IMAGE_IMPLEMENTATION
#include <Libraries/stb_image.h>
#endif

PNG* PNG::Load(Stream* stream) {
#ifdef USING_LIBPNG
	PNG* png = new PNG;

	// PNG variables
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	Uint32 width, height;
	int bit_depth, color_type, palette_size;
	bool usePalette = false;
	png_colorp palette;
	Uint8* pixelData = NULL;
	png_bytep* row_pointers = NULL;
	Uint32 row_bytes;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		Log::Print(Log::LOG_ERROR, "Couldn't create PNG read struct!");
		goto PNG_Load_FAIL;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		Log::Print(Log::LOG_ERROR, "Couldn't create PNG information struct!");
		goto PNG_Load_FAIL;
	}

	if (setjmp(*png_set_longjmp_fn(png_ptr, longjmp, sizeof(jmp_buf)))) {
		Log::Print(Log::LOG_ERROR, "Couldn't read PNG file!");
		goto PNG_Load_FAIL;
	}

	png_set_read_fn(png_ptr, stream, png_read_fn);
	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);
	if (bit_depth == 16) {
		png_set_strip_16(png_ptr);
	}

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb(png_ptr);
	}
	else if (color_type == PNG_COLOR_TYPE_PALETTE) {
		if (Graphics::UsePalettes &&
			png_get_PLTE(png_ptr, info_ptr, &palette, &palette_size)) {
			usePalette = true;
		}

		if (!usePalette) {
			png_set_palette_to_rgb(png_ptr);
		}
	}

	if (!usePalette && png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png_ptr);
	}
	else if (color_type != PNG_COLOR_TYPE_RGB_ALPHA &&
		color_type != PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
	}

	png_read_update_info(png_ptr, info_ptr);

	png->Width = width;
	png->Height = height;
	png->BytesPerPixel = usePalette ? sizeof(Uint8) : sizeof(Uint32);
	png->Data = (Uint8*)Memory::TrackedMalloc("PNG::Data", width * height * png->BytesPerPixel);

	row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * png->Height);
	if (!row_pointers) {
		goto PNG_Load_FAIL;
	}

	pixelData = (Uint8*)malloc(width * height * png->BytesPerPixel);
	row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	for (Uint32 row = 0, pitch = row_bytes; row < png->Height; row++) {
		row_pointers[row] = (png_bytep)(pixelData + row * pitch);
	}

	png_read_image(png_ptr, row_pointers);
	free(row_pointers);

	if (usePalette) {
		if (bit_depth != 8) {
			png->ReadPixelBitstream(pixelData, bit_depth);
		}
		else {
			for (size_t i = 0; i < width * height; i++) {
				png->Data[i] = pixelData[i];
			}
		}

		png->NumPaletteColors = palette_size;
		png->Colors = (Uint32*)Memory::TrackedMalloc(
			"PNG::Colors", png->NumPaletteColors * sizeof(Uint32));
		png->Paletted = true;

		for (size_t i = 0; i < png->NumPaletteColors; i++, palette++) {
			png->Colors[i] = ColorUtils::Make(palette->red, palette->green, palette->blue, 0xFF, Graphics::PreferredPixelFormat);
		}
	}
	else {
		int num_channels = PNG_COLOR_TYPE_RGB_ALPHA ? 4 : 3;
		png->ReadPixelData(pixelData, num_channels);
		png->Paletted = false;
		png->NumPaletteColors = 0;
	}

	free(pixelData);

	goto PNG_Load_Success;

PNG_Load_FAIL:
	delete png;
	png = NULL;

PNG_Load_Success:
	if (png_ptr) {
		png_destroy_read_struct(
			&png_ptr, info_ptr ? &info_ptr : (png_infopp)NULL, (png_infopp)NULL);
	}
	return png;
#elif defined(USING_SPNG)
	PNG* png = new PNG;
	Uint8* pixelData = NULL;
	Uint8* buffer = NULL;
	size_t buffer_len = 0;
	int fmt = SPNG_FMT_PNG;
	int color_type, flags = SPNG_DECODE_TRNS;
	bool isIndexed;
	bool useTransparency = false;
	bool usePalette = false;
	struct spng_plte plte = {0};
	spng_ctx* ctx = NULL;
	size_t limit = 1024 * 1024 * 64;
	size_t image_size;
	int ret;

	buffer_len = stream->Length();
	buffer = (Uint8*)malloc(buffer_len);
	stream->ReadBytes(buffer, buffer_len);

	ctx = spng_ctx_new(0);
	if (ctx == NULL) {
		Log::Print(Log::LOG_ERROR, "spng_ctx_new() failed!");
		goto PNG_Load_FAIL;
	}

	spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);

	spng_set_chunk_limits(ctx, limit, limit);
	spng_set_png_buffer(ctx, buffer, buffer_len);

	struct spng_ihdr ihdr;
	ret = spng_get_ihdr(ctx, &ihdr);
	if (ret) {
		Log::Print(Log::LOG_ERROR, "spng_ctx_new() failed! %s", spng_strerror(ret));
		goto PNG_Load_FAIL;
	}

	png->Width = ihdr.width;
	png->Height = ihdr.height;

	color_type = ihdr.color_type;
	isIndexed = color_type == SPNG_COLOR_TYPE_INDEXED;
	if (isIndexed && Graphics::UsePalettes) {
		ret = spng_get_plte(ctx, &plte);

		if (ret && ret != SPNG_ECHUNKAVAIL) {
			Log::Print(
				Log::LOG_ERROR, "spng_get_plte() failed! %s", spng_strerror(ret));
			goto PNG_Load_FAIL;
		}

		usePalette = !ret;
	}

	if (!usePalette) {
		fmt = SPNG_FMT_RGBA8;
	}

	ret = spng_decoded_image_size(ctx, fmt, &image_size);
	if (ret) {
		goto PNG_Load_FAIL;
	}

	pixelData = (Uint8*)malloc(image_size);
	if (pixelData == NULL) {
		goto PNG_Load_FAIL;
	}

	ret = spng_decode_image(ctx, (Uint8*)pixelData, image_size, fmt, flags);
	if (ret) {
		Log::Print(Log::LOG_ERROR, "spng_decode_image() failed! %s", spng_strerror(ret));
		goto PNG_Load_FAIL;
	}

	png->BytesPerPixel = usePalette ? sizeof(Uint8) : sizeof(Uint32);
	png->Data = (Uint32*)Memory::TrackedMalloc(
		"PNG::Data", ihdr.width * ihdr.height * png->BytesPerPixel);

	if (usePalette) {
		if (ihdr.bit_depth != 8) {
			png->ReadPixelBitstream(pixelData, ihdr.bit_depth);
		}
		else {
			for (size_t i = 0; i < ihdr.width * ihdr.height; i++) {
				png->Data[i] = pixelData[i];
			}
		}

		png->NumPaletteColors = plte.n_entries;
		png->Colors = (Uint32*)Memory::TrackedMalloc(
			"PNG::Colors", png->NumPaletteColors * sizeof(Uint32));
		png->Paletted = true;

		for (size_t i = 0; i < png->NumPaletteColors; i++) {
			png->Colors[i] = 0xFF000000U;
			png->Colors[i] |= plte.entries[i].red << 16;
			png->Colors[i] |= plte.entries[i].green << 8;
			png->Colors[i] |= plte.entries[i].blue;
		}

		Graphics::ConvertFromARGBtoNative(png->Colors, png->NumPaletteColors);
	}
	else {
		png->ReadPixelData(pixelData, 4);
		png->Paletted = false;
		png->Colors = nullptr;
	}

	goto PNG_Load_Success;

PNG_Load_FAIL:
	delete png;
	png = NULL;

PNG_Load_Success:
	if (buffer) {
		free(buffer);
	}
	if (ctx) {
		spng_ctx_free(ctx);
	}
	free(pixelData);
	return png;
#else
	PNG* png = new PNG;
	Uint8* pixelData = NULL;
	int num_channels;
	int width, height;
	Uint8* buffer = NULL;
	size_t buffer_len = 0;

	buffer_len = stream->Length();
	buffer = (Uint8*)malloc(buffer_len);
	stream->ReadBytes(buffer, buffer_len);

	pixelData = (Uint8*)stbi_load_from_memory(
		buffer, buffer_len, &width, &height, &num_channels, STBI_rgb_alpha);
	if (!pixelData) {
		Log::Print(Log::LOG_ERROR, "stbi_load failed: %s", stbi_failure_reason());
		goto PNG_Load_FAIL;
	}

	png->Width = width;
	png->Height = height;
	png->BytesPerPixel = sizeof(Uint32);
	png->Data = (Uint32*)Memory::TrackedMalloc(
		"PNG::Data", png->Width * png->Height * png->BytesPerPixel);

	png->ReadPixelData(pixelData, num_channels);

	png->Colors = nullptr;
	png->Paletted = false;
	png->NumPaletteColors = 0;

	goto PNG_Load_Success;

PNG_Load_FAIL:
	delete png;
	png = NULL;

PNG_Load_Success:
	if (buffer) {
		free(buffer);
	}
	if (pixelData) {
		stbi_image_free(pixelData);
	}
	return png;
#endif
	return NULL;
}
void PNG::ReadPixelData(Uint8* pixelData, int num_channels) {
	Uint32* dest = (Uint32*)Data;

	if (Graphics::PreferredPixelFormat == PixelFormat_RGBA8888 && num_channels == 4) {
		memcpy(dest, pixelData, Width * Height * sizeof(Uint32));
		return;
	}

	int pc = 0, size = Width * Height;
	if (num_channels == 4) {
		for (Uint32* px = (Uint32*)pixelData; pc < size; px++, pc++) {
			dest[pc] = ColorUtils::Convert(*px, PixelFormat_RGBA8888, Graphics::PreferredPixelFormat);
		}
	}
	else {
		for (Uint8* px = pixelData; pc < size; px += num_channels, pc++) {
			dest[pc] = ColorUtils::Make(px[0], px[1], px[2], 0xFF, Graphics::PreferredPixelFormat);
		}
	}
}
void PNG::ReadPixelBitstream(Uint8* pixelData, size_t bit_depth) {
	size_t scanline_width = (((bit_depth * Width) + 15) / 8) - 1;
	Uint8 mask = (1 << bit_depth) - 1;

	for (size_t y = 0; y < Height; y++) {
		Uint8* src = pixelData + (y * scanline_width);
		Uint8* out = Data + (y * Width);

		memset(out, 0, Width * sizeof(Uint8));

		for (size_t w = 0, r = 0; r < scanline_width;) {
			Uint8 pixels = src[r++];
			Uint8 buf[8];
			size_t i = 0;

			for (size_t b = 0; b < 8; b += bit_depth) {
				buf[i++] = pixels & mask;
				pixels >>= bit_depth;
			}
			--i;

			for (size_t b = 0; b < 8; b += bit_depth) {
				out[w++] = buf[i--];
			}
		}
	}
}

bool PNG::Save(PNG* png, const char* filename) {
	return png->Save(filename);
}

bool PNG::Save(const char* filename) {
	Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
	if (!stream) {
		return false;
	}

	stream->Close();
	return true;
}

PNG::~PNG() {}
