#include <Engine/ResourceTypes/ImageFormats/PNG.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Graphics.h>
#include <Engine/IO/FileStream.h>

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

#ifdef USING_LIBPNG
#ifdef PNG_WRITE_SUPPORTED
#define CAN_SAVE_PNG
void png_write_fn(png_structp ctx, png_bytep area, png_size_t size) {
	Stream* stream = (Stream*)png_get_io_ptr(ctx);
	stream->WriteBytes(area, size);
}
#endif
#endif

#ifndef USING_LIBPNG
#define USING_SPNG
#define CAN_SAVE_PNG
#endif

#ifdef USING_SPNG
#define SPNG_STATIC
#ifndef _WIN32
#define SPNG_USE_SIMD
#endif
#include <Libraries/spng.h>
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
	bool hasTransparency = false;
	bool hasTransValues = false;
	png_bytep trans;
	int num_trans;
	png_color_16p trans_values;
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

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		hasTransparency = true;

		if (png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, &trans_values) == PNG_INFO_tRNS) {
			hasTransValues = true;
		}
	}

	if (!usePalette && hasTransparency) {
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
		Uint8* data = pixelData;
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
			Uint8 alpha = 0xFF;
			if (hasTransValues && i < (size_t)num_trans) {
				alpha = trans[i];
			}

			png->Colors[i] = ColorUtils::Make(palette->red, palette->green, palette->blue, alpha, Graphics::PreferredPixelFormat);
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

	spng_ihdr ihdr;
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
	png->Data = (Uint8*)Memory::TrackedMalloc(
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
	return NULL;
#endif
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
	return false;
}

#ifdef CAN_SAVE_PNG
#if defined(USING_LIBPNG) && defined(PNG_TEXT_SUPPORTED)
static void
AddTextLibpng(png_structp png_ptr, png_infop png_info_ptr, std::vector<PNGMetadata>* metadata) {
	size_t numMetadata = metadata->size();
	if (numMetadata == 0) {
		return;
	}

	png_text* png_text_p = (png_text*)Memory::Calloc(numMetadata, sizeof(png_text));
	size_t numStrings = 0;

	for (size_t i = 0; i < numMetadata; i++) {
		char* keyword = (*metadata)[i].Keyword;
		std::string& textString = (*metadata)[i].Text;
		if (keyword[0] == '\0') {
			continue;
		}

		png_text_p[numStrings].key = keyword;
		png_text_p[numStrings].text = (char*)textString.c_str();

		numStrings++;
	}

	if (numStrings > 0) {
		png_set_text(png_ptr, png_info_ptr, png_text_p, numStrings);
	}

	Memory::Free(png_text_p);
}
#elif defined(USING_SPNG)
static void AddTextLibspng(spng_ctx* ctx, std::vector<PNGMetadata>* metadata) {
	size_t numMetadata = metadata->size();
	if (numMetadata == 0) {
		return;
	}

	spng_text* text = (spng_text*)Memory::Calloc(numMetadata, sizeof(spng_text));
	uint32_t n_text = 0;

	for (size_t i = 0; i < numMetadata; i++) {
		char* keyword = (*metadata)[i].Keyword;
		std::string& textString = (*metadata)[i].Text;
		size_t textLength = textString.size();
		if (keyword[0] == '\0' || textLength == 0) {
			continue;
		}

		memcpy(text[n_text].keyword, keyword, sizeof(text[n_text].keyword));
		text[n_text].text = (char*)textString.c_str();
		text[n_text].length = textLength;
		text[n_text].type = SPNG_TEXT;

		n_text++;
	}

	if (n_text > 0) {
		int ret = spng_set_text(ctx, text, n_text);
		if (ret) {
			Log::Print(Log::LOG_WARN, "spng_set_text() failed! %s", spng_strerror(ret));
		}
	}

	Memory::Free(text);
}
#endif
#endif

bool PNG::Save(Texture* texture, const char* filename, std::vector<PNGMetadata>* metadata) {
#ifdef CAN_SAVE_PNG
	Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS, true);
	if (!stream) {
		return false;
	}

#ifdef USING_LIBPNG
	png_structp png_ptr;
	png_infop png_info_ptr;
	png_bytepp row_pointers;
	png_bytep data_ptr;
	Uint32 row_bytes;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		Log::Print(Log::LOG_ERROR, "Couldn't create PNG write struct!");
		return false;
	}

	png_info_ptr = png_create_info_struct(png_ptr);
	if (!png_info_ptr) {
		Log::Print(Log::LOG_ERROR, "Couldn't create PNG information struct!");
		png_destroy_write_struct(&png_ptr, NULL);
		return false;
	}

	if (setjmp(*png_set_longjmp_fn(png_ptr, longjmp, sizeof(jmp_buf)))) {
		Log::Print(Log::LOG_ERROR, "Couldn't write PNG file!");
		png_destroy_write_struct(&png_ptr, &png_info_ptr);
		stream->Close();
		return false;
	}

	// TODO: Write the PNG as PNG_COLOR_TYPE_PALETTE if the Texture is palettized
	png_set_write_fn(png_ptr, stream, png_write_fn, NULL);
	png_set_IHDR(png_ptr,
		png_info_ptr,
		texture->Width,
		texture->Height,
		8,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);
#ifdef PNG_TEXT_SUPPORTED
	if (metadata) {
		AddTextLibpng(png_ptr, png_info_ptr, metadata);
	}
#endif
	png_write_info(png_ptr, png_info_ptr);

	row_pointers = (png_bytepp)Memory::Malloc(texture->Height * sizeof(png_bytep));
	row_bytes = texture->Width * sizeof(Uint32);
	data_ptr = (png_bytep)texture->Pixels;

	for (int y = 0; y < texture->Height; y++) {
		row_pointers[y] = data_ptr;
		data_ptr += row_bytes;
	}

	png_write_image(png_ptr, row_pointers);
	Memory::Free(row_pointers);

	png_write_end(png_ptr, png_info_ptr);
	png_destroy_write_struct(&png_ptr, &png_info_ptr);
#elif defined(USING_SPNG)
	spng_ctx* ctx;
	spng_ihdr ihdr = {0};

	int ret;
	void* outData;
	size_t outSize;

	ctx = spng_ctx_new(SPNG_CTX_ENCODER);
	if (ctx == NULL) {
		Log::Print(Log::LOG_ERROR, "spng_ctx_new() failed!");
		goto PNG_Save_FAIL;
	}

	// TODO: Use SPNG_COLOR_TYPE_INDEXED and spng_set_plte() if the Texture is palettized
	ihdr.width = texture->Width;
	ihdr.height = texture->Height;
	ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
	ihdr.bit_depth = 8;

	spng_set_ihdr(ctx, &ihdr);
	spng_set_option(ctx, SPNG_ENCODE_TO_BUFFER, 1);

	if (metadata) {
		AddTextLibspng(ctx, metadata);
	}

	ret = spng_encode_image(ctx,
		texture->Pixels,
		texture->Width * texture->Height * sizeof(Uint32),
		SPNG_FMT_PNG,
		SPNG_ENCODE_FINALIZE);

	if (ret) {
		Log::Print(Log::LOG_ERROR, "spng_encode_image() failed! %s", spng_strerror(ret));
		goto PNG_Save_FAIL;
	}

	outData = spng_get_png_buffer(ctx, &outSize, &ret);
	if (outData == NULL) {
		Log::Print(Log::LOG_ERROR, "spng_get_png_buffer() failed! %s", spng_strerror(ret));
		goto PNG_Save_FAIL;
	}

	stream->WriteBytes(outData, outSize);

	free(outData);

PNG_Save_FAIL:
	spng_ctx_free(ctx);
#endif

	stream->Close();
	return true;
#else
	Log::Print(Log::LOG_ERROR, "Cannot save PNG files in this build!");
	return false;
#endif
}
bool PNG::Save(Texture* texture, const char* filename) {
	return Save(texture, filename, nullptr);
}

PNG::~PNG() {}
