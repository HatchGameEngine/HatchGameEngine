#include <Engine/Media/Decoder.h>

#include <Engine/Diagnostics/Log.h>

#define BUFFER_IN_SIZE 256

#ifdef USING_LIBAV

enum { KIT_DEC_BUF_IN = 0, KIT_DEC_BUF_OUT, KIT_DEC_BUF_COUNT };

typedef void (*FreePacketCallback)(void*);

// void (*)(void*)
void Decoder::FreeInVideoPacketFunc(void* packet) {
	av_packet_free((AVPacket**)&packet);
}

void Decoder::Create(MediaSource* src,
	int stream_index,
	int outBufferLength,
	void (*freeOutFunc)(void*),
	int thread_count) {
	if (outBufferLength <= 0) {
		Log::Print(Log::LOG_ERROR, "Decoder::Create: outBufferLength <= 0");
		exit(-1);
	}
	if (thread_count <= 0) {
		Log::Print(Log::LOG_ERROR, "Decoder::Create: thread_count <= 0");
		exit(-1);
	}

	Successful = false;
	this->DecodeFunc = NULL;
	this->CloseFunc = NULL;

	AVCodecContext* codec_ctx = NULL;
	AVDictionary* codec_opts = NULL;
	AVCodec* codec = NULL;
	AVFormatContext* format_ctx = (AVFormatContext*)src->FormatCtx;
	int bsizes[2] = {BUFFER_IN_SIZE, outBufferLength};
	FreePacketCallback free_hooks[2] = {Decoder::FreeInVideoPacketFunc, freeOutFunc};

	// Make sure index seems correct
	if (stream_index >= (int)format_ctx->nb_streams || stream_index < 0) {
		Log::Print(Log::LOG_ERROR, "Invalid stream %d", stream_index);
		goto exit_0;
	}

// Find audio decoder
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 48, 101)
	codec = avcodec_find_decoder(format_ctx->streams[stream_index]->codec->codec_id);
#else
	codec = avcodec_find_decoder(format_ctx->streams[stream_index]->codecpar->codec_id);
#endif
	if (codec == NULL) {
		Log::Print(Log::LOG_ERROR, "No suitable decoder found for stream %d", stream_index);
		goto exit_1;
	}

	// Allocate a context for the codec
	codec_ctx = avcodec_alloc_context3(codec);
	if (codec_ctx == NULL) {
		Log::Print(Log::LOG_ERROR,
			"Unable to allocate codec context for stream %d",
			stream_index);
		goto exit_1;
	}

// Copy params
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 48, 101)
	if (avcodec_copy_context(codec_ctx, format_ctx->streams[stream_index]->codec) != 0)
#else
	if (avcodec_parameters_to_context(codec_ctx, format_ctx->streams[stream_index]->codecpar) <
		0)
#endif
	{
		Log::Print(
			Log::LOG_ERROR, "Unable to copy codec context for stream %d", stream_index);
		goto exit_2;
	}

// NOTE: Required by ffmpeg for now when using the new API.
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	codec_ctx->pkt_timebase = format_ctx->streams[stream_index]->time_base;
#endif

	// Set thread count
	codec_ctx->thread_count = thread_count;
	codec_ctx->thread_type = FF_THREAD_SLICE | FF_THREAD_FRAME;

// NOTE: This is required for ass_process_chunk() support
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 25, 100)
	av_dict_set(&codec_opts, "sub_text_format", "ass", 0);
#endif

	// Open the stream
	if (avcodec_open2(codec_ctx, codec, &codec_opts) < 0) {
		Log::Print(Log::LOG_ERROR, "Unable to open codec for stream %d", stream_index);
		goto exit_2;
	}

	// Set index and codec
	CodecCtx = codec_ctx;
	FormatCtx = format_ctx;
	StreamIndex = stream_index;

	// Allocate input/output ringbuffers
	for (int i = 0; i < KIT_DEC_BUF_COUNT; i++) {
		Buffer[i] = new PtrBuffer(bsizes[i], free_hooks[i]);
		if (Buffer[i] == NULL) {
			Log::Print(Log::LOG_ERROR,
				"Unable to allocate buffer for stream %d: %s",
				stream_index,
				SDL_GetError());
			goto exit_3;
		}
	}

	// Create a lock for output buffer synchronization
	OutputLock = SDL_CreateMutex();
	if (OutputLock == NULL) {
		Log::Print(Log::LOG_ERROR,
			"Unable to allocate mutex for stream %d: %s",
			stream_index,
			SDL_GetError());
		goto exit_3;
	}

	// That's that
	return;

exit_3:
	for (int i = 0; i < KIT_DEC_BUF_COUNT; i++) {
		delete Buffer[i];
	}
	avcodec_close(codec_ctx);
exit_2:
	av_dict_free(&codec_opts);
	avcodec_free_context(&codec_ctx);
exit_1:
// free(dec);
exit_0:
	return;
}
void Decoder::Close() {
	if (CloseFunc) {
		CloseFunc(this);
	}

	for (int i = 0; i < KIT_DEC_BUF_COUNT; i++) {
		delete Buffer[i];
	}
	SDL_DestroyMutex(OutputLock);
	avcodec_close(CodecCtx);
	avcodec_free_context(&CodecCtx);
}
Decoder::~Decoder() {
	Close();
}
int Decoder::Run() {
	AVPacket* in_packet;
	int is_output_full = 1;

	// First, check if there is room in output buffer
	if (SDL_LockMutex(OutputLock) == 0) {
		is_output_full = Buffer[KIT_DEC_BUF_OUT]->IsFull();
		SDL_UnlockMutex(OutputLock);
	}

	if (is_output_full) {
		return 0;
	}

	// Then, see if we have incoming data
	in_packet = PeekInput();
	if (in_packet == NULL) {
		return 0;
	}

	// Run decoder with incoming packet
	if (DecodeFunc(this, in_packet) == 0) {
		AdvanceInput();
		av_packet_free(&in_packet);
		return 1;
	}
	return 0;
}
// ---- Information API ----
int Decoder::GetCodecInfo(Codec* codec) {
	codec->Threads = CodecCtx->thread_count;
	strncpy(codec->Name, CodecCtx->codec->name, KIT_CODEC_NAME_MAX - 1);
	strncpy(codec->Description, CodecCtx->codec->long_name, KIT_CODEC_DESC_MAX - 1);
	return 0;
}
int Decoder::GetOutputFormat(OutputFormat* output) {
	output->Format = Format;
	return 0;
}
int Decoder::GetStreamIndex() {
	return StreamIndex;
}
// ---- Clock handling ----
void Decoder::SetClockSync(double sync) {
	ClockSync = sync;
}
void Decoder::ChangeClockSync(double sync) {
	ClockSync += sync;
}
// ---- Input buffer handling ----
int Decoder::WriteInput(AVPacket* packet) {
	return Buffer[KIT_DEC_BUF_IN]->Write(packet);
}
AVPacket* Decoder::PeekInput() {
	return (AVPacket*)Buffer[KIT_DEC_BUF_IN]->Peek();
}
AVPacket* Decoder::ReadInput() {
	return (AVPacket*)Buffer[KIT_DEC_BUF_IN]->Read();
}
bool Decoder::CanWriteInput() {
	return !(Buffer[KIT_DEC_BUF_IN]->IsFull());
}
void Decoder::AdvanceInput() {
	Buffer[KIT_DEC_BUF_IN]->Advance();
}
void Decoder::ClearInput() {
	Buffer[KIT_DEC_BUF_IN]->Clear();
}
// ---- Output buffer handling ----
// Kit_([A-z0-9_]+)Buffer([A-z0-9_]*)\((.*)\)
int Decoder::WriteOutput(void* packet) {
	int ret = 1;
	if (SDL_LockMutex(OutputLock) == 0) {
		ret = Buffer[KIT_DEC_BUF_OUT]->Write(packet);
		SDL_UnlockMutex(OutputLock);
	}
	return ret;
}
void* Decoder::PeekOutput() {
	void* ret = NULL;
	if (SDL_LockMutex(OutputLock) == 0) {
		ret = Buffer[KIT_DEC_BUF_OUT]->Peek();
		SDL_UnlockMutex(OutputLock);
	}
	return ret;
}
void* Decoder::ReadOutput() {
	void* ret = NULL;
	if (SDL_LockMutex(OutputLock) == 0) {
		ret = Buffer[KIT_DEC_BUF_OUT]->Read();
		SDL_UnlockMutex(OutputLock);
	}
	return ret;
}
bool Decoder::CanWriteOutput() {
	bool ret = false;
	if (SDL_LockMutex(OutputLock) == 0) {
		ret = !(Buffer[KIT_DEC_BUF_OUT]->IsFull());
		SDL_UnlockMutex(OutputLock);
	}
	return ret;
}
void Decoder::AdvanceOutput() {
	if (SDL_LockMutex(OutputLock) == 0) {
		Buffer[KIT_DEC_BUF_OUT]->Advance();
		SDL_UnlockMutex(OutputLock);
	}
}
void Decoder::ClearOutput() {
	if (SDL_LockMutex(OutputLock) == 0) {
		Buffer[KIT_DEC_BUF_OUT]->Clear();
		SDL_UnlockMutex(OutputLock);
	}
}

void Decoder::ForEachOutput(void (*cb)(void*, void*), void* userdata) {
	if (SDL_LockMutex(OutputLock) == 0) {
		Buffer[KIT_DEC_BUF_OUT]->ForEachItemInBuffer(cb, userdata);
		SDL_UnlockMutex(OutputLock);
	}
}
Uint32 Decoder::GetInputLength() {
	return Buffer[KIT_DEC_BUF_IN]->GetLength();
}
Uint32 Decoder::GetOutputLength() {
	Uint32 len = 0;
	if (SDL_LockMutex(OutputLock) == 0) {
		len = Buffer[KIT_DEC_BUF_OUT]->GetLength();
		SDL_UnlockMutex(OutputLock);
	}
	return len;
}
void Decoder::ClearBuffers() {
	ClearInput();
	ClearOutput();
	avcodec_flush_buffers(CodecCtx);
}

int Decoder::LockOutput() {
	return SDL_LockMutex(OutputLock);
}
void Decoder::UnlockOutput() {
	SDL_UnlockMutex(OutputLock);
}

#endif
