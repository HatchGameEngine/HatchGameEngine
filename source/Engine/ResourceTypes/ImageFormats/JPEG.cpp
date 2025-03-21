#include <Engine/ResourceTypes/ImageFormats/JPEG.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Graphics.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/Scene.h>

#ifdef USING_LIBJPEG
extern "C" {
#include <jpeglib.h>
#include <setjmp.h>
}
#define FAST_IS_JPEG

#define INPUT_BUFFER_SIZE 4096
struct CustomSourceManager {
	jpeg_source_mgr pub;

	Stream* stream;
	Uint8 buffer[INPUT_BUFFER_SIZE];
};
struct CustomErrorManager {
	jpeg_error_mgr mgr;
	jmp_buf escape;
};

static void CustomErrorExit(j_common_ptr cinfo) {
	CustomErrorManager* err = (CustomErrorManager*)cinfo->err;
	longjmp(err->escape, 1);
}
static void CustomOutputMessage(j_common_ptr cinfo) {}

static void init_source(j_decompress_ptr cinfo) {}
static boolean fill_input_buffer(j_decompress_ptr cinfo) {
	CustomSourceManager* src = (CustomSourceManager*)cinfo->src;
	int nbytes;

	nbytes = (int)src->stream->ReadBytes(src->buffer, INPUT_BUFFER_SIZE);
	if (nbytes <= 0) {
		src->buffer[0] = (Uint8)0xFF;
		src->buffer[1] = (Uint8)JPEG_EOI;
		nbytes = 2;
	}
	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;

	return TRUE;
}
static void skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
	CustomSourceManager* src = (CustomSourceManager*)cinfo->src;

	if (num_bytes > 0) {
		while (num_bytes > (long)src->pub.bytes_in_buffer) {
			num_bytes -= (long)src->pub.bytes_in_buffer;
			(void)src->pub.fill_input_buffer(cinfo);
		}
		src->pub.next_input_byte += (size_t)num_bytes;
		src->pub.bytes_in_buffer -= (size_t)num_bytes;
	}
}
static void term_source(j_decompress_ptr cinfo) {}
static void jpeg_Hatch_IO_src(j_decompress_ptr cinfo, Stream* stream) {
	CustomSourceManager* src;

	/* The source object and input buffer are made permanent so
	 * that a series of JPEG images can be read from the same file
	 * by calling jpeg_stdio_src only before the first one.  (If we
	 * discarded the buffer at the end of one image, we'd likely
	 * lose the start of the next one.) This makes it unsafe to use
	 * this manager and a different source manager serially with
	 * the same JPEG object.  Caveat programmer.
	 */
	if (cinfo->src == NULL) { /* first time for this JPEG object? */
		cinfo->src = (jpeg_source_mgr*)(*cinfo->mem->alloc_small)(
			(j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(CustomSourceManager));
		// src = (CustomSourceManager*)cinfo->src;
	}

	src = (CustomSourceManager*)cinfo->src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
	src->pub.term_source = term_source;
	src->stream = stream;
	src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
	src->pub.next_input_byte = NULL; /* until buffer loaded */
}
#endif

JPEG* JPEG::Load(const char* filename) {
#ifdef USING_LIBJPEG
	JPEG* jpeg = new JPEG;
	Stream* stream = NULL;
	// size_t fileSize;
	// void*  fileBuffer = NULL;
	// SDL_Surface* temp = NULL,* tempOld = NULL;

	// JPEG variables
	Sint64 start;
	JSAMPROW rowptr[1];
	CustomErrorManager jerr;
	jpeg_decompress_struct cinfo;
	Uint32 *pixelData, pitch, num_channels;
	bool cinfoFlag = false;

	Uint32 Rmask, Gmask, Bmask, Amask;
	bool doConvert;

	stream = ResourceStream::New(filename);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Could not open file '%s'!", filename);
		goto JPEG_Load_FAIL;
	}

	start = stream->Position();

	cinfoFlag = true;

	/* Create a decompression structure and load the JPEG header */
	cinfo.err = jpeg_std_error(&jerr.mgr);
	jerr.mgr.error_exit = CustomErrorExit;
	jerr.mgr.output_message = CustomOutputMessage;
	// jmp_buf
	if (setjmp(jerr.escape)) {
		/* If we get here, libjpeg found an error */
		if (jpeg->Data != NULL) {
			Memory::Free(jpeg->Data);
		}
		stream->Seek(start);
		Log::Print(Log::LOG_ERROR, "JPEG loading error");
		goto JPEG_Load_FAIL;
	}

	jpeg_create_decompress(&cinfo);
	jpeg_Hatch_IO_src(&cinfo, stream);
	jpeg_read_header(&cinfo, TRUE);

	if (cinfo.num_components == 4) {
		/* Set 32-bit Raw output */
		cinfo.out_color_space = JCS_CMYK;
		cinfo.quantize_colors = FALSE;
		jpeg_calc_output_dimensions(&cinfo);
	}
	else {
		/* Set 24-bit RGB output */
		cinfo.out_color_space = JCS_RGB;
		cinfo.quantize_colors = FALSE;
#ifdef FAST_JPEG
		cinfo.scale_num = 1;
		cinfo.scale_denom = 1;
		cinfo.dct_method = JDCT_FASTEST;
		cinfo.do_fancy_upsampling = FALSE;
#endif
		jpeg_calc_output_dimensions(&cinfo);
	}

	jpeg->Width = cinfo.output_width;
	jpeg->Height = cinfo.output_height;
	jpeg->Paletted = false;
	jpeg->NumPaletteColors = 0;

	pixelData = (Uint32*)malloc(jpeg->Width * jpeg->Height * cinfo.num_components);
	pitch = jpeg->Width * cinfo.num_components;
	num_channels = cinfo.num_components;

	if (!pixelData) {
		stream->Seek(start);
		Log::Print(Log::LOG_ERROR, "Out of memory");
		goto JPEG_Load_FAIL;
	}

	jpeg->Data = (Uint32*)Memory::TrackedMalloc(
		"JPEG::Data", jpeg->Width * jpeg->Height * sizeof(Uint32));
	for (Uint32 i = 0; i < jpeg->Width * (jpeg->Height / 2); i++) {
		jpeg->Data[i] = 0xFF41D1F2;
	}

	// /* Decompress the image */
	jpeg_start_decompress(&cinfo);
	while (cinfo.output_scanline < cinfo.output_height) {
		rowptr[0] = (JSAMPROW)(Uint8*)pixelData + cinfo.output_scanline * pitch;
		jpeg_read_scanlines(&cinfo, rowptr, (JDIMENSION)1);
	}
	jpeg_finish_decompress(&cinfo);

	doConvert = false;
	if (Graphics::PreferredPixelFormat == SDL_PIXELFORMAT_ABGR8888) {
		Amask = (num_channels == 4) ? 0xFF000000 : 0;
		Bmask = 0x00FF0000;
		Gmask = 0x0000FF00;
		Rmask = 0x000000FF;
		if (num_channels != 4) {
			doConvert = true;
		}
	}
	else if (Graphics::PreferredPixelFormat == SDL_PIXELFORMAT_ARGB8888) {
		Amask = (num_channels == 4) ? 0xFF000000 : 0;
		Rmask = 0x00FF0000;
		Gmask = 0x0000FF00;
		Bmask = 0x000000FF;
		doConvert = true;
	}

	if (doConvert) {
		Uint8* px;
		Uint32 a, b, g, r;
		int pc = 0, size = jpeg->Width * jpeg->Height;
		if (Amask) {
			for (Uint32* i = pixelData; pc < size; i++, pc++) {
				px = (Uint8*)i;
				r = px[0] | px[0] << 8 | px[0] << 16 | px[0] << 24;
				g = px[1] | px[1] << 8 | px[1] << 16 | px[1] << 24;
				b = px[2] | px[2] << 8 | px[2] << 16 | px[2] << 24;
				a = px[3] | px[3] << 8 | px[3] << 16 | px[3] << 24;
				jpeg->Data[pc] =
					(r & Rmask) | (g & Gmask) | (b & Bmask) | (a & Amask);
			}
		}
		else {
			for (Uint8* i = (Uint8*)pixelData; pc < size; i += 3, pc++) {
				px = (Uint8*)i;
				r = px[0] | px[0] << 8 | px[0] << 16 | px[0] << 24;
				g = px[1] | px[1] << 8 | px[1] << 16 | px[1] << 24;
				b = px[2] | px[2] << 8 | px[2] << 16 | px[2] << 24;
				jpeg->Data[pc] =
					(r & Rmask) | (g & Gmask) | (b & Bmask) | 0xFF000000;
			}
		}
	}
	else {
		memcpy(jpeg->Data, pixelData, jpeg->Width * jpeg->Height * sizeof(Uint32));
		// memcpy(jpeg->Data, pixelData, jpeg->Width *
		// jpeg->Height * 3);
	}
	free(pixelData);

	goto JPEG_Load_Success;

JPEG_Load_FAIL:
	delete jpeg;
	jpeg = NULL;

JPEG_Load_Success:
	if (cinfoFlag) {
		jpeg_destroy_decompress(&cinfo);
	}
	if (stream) {
		stream->Close();
	}
	return jpeg;
#endif
	return NULL;
}
bool JPEG::Save(JPEG* jpeg, const char* filename) {
	return jpeg->Save(filename);
}

bool JPEG::Save(const char* filename) {
	Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
	if (!stream) {
		return false;
	}

	stream->Close();
	return true;
}

JPEG::~JPEG() {}
