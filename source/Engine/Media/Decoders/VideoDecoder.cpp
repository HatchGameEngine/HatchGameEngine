#include <Engine/Media/Decoders/VideoDecoder.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Graphics.h>
#include <Engine/Media/Includes/AVUtils.h>
#include <Engine/Media/Utils/MediaPlayerState.h>

#define KIT_VIDEO_SYNC_THRESHOLD 0.02

#ifdef USING_LIBAV

enum AVPixelFormat supported_list[] = {AV_PIX_FMT_YUV420P,
	AV_PIX_FMT_YUYV422,
	AV_PIX_FMT_UYVY422,
	AV_PIX_FMT_NV12,
	AV_PIX_FMT_NV21,
	AV_PIX_FMT_RGB24,
	AV_PIX_FMT_BGR24,
	AV_PIX_FMT_RGB555,
	AV_PIX_FMT_BGR555,
	AV_PIX_FMT_RGB565,
	AV_PIX_FMT_BGR565,
	AV_PIX_FMT_BGRA,
	AV_PIX_FMT_RGBA,
	AV_PIX_FMT_NONE};

struct VideoPacket {
	double pts;
	AVFrame* frame;
};

// Lifecycle functions
VideoDecoder::VideoDecoder(MediaSource* src, int stream_index) {
	if (stream_index < 0) {
		Log::Print(Log::LOG_ERROR, "stream_index < 0");
		return;
	}

	Decoder::Create(src,
		stream_index,
		MediaPlayerState::VideoBufFrames,
		VideoDecoder::FreeVideoPacket,
		MediaPlayerState::ThreadCount);

	AVPixelFormat output_format;

	ScratchFrame = av_frame_alloc();
	if (ScratchFrame == NULL) {
		Log::Print(Log::LOG_ERROR, "Unable to initialize temporary video frame");
		exit(-1);
		goto exit_2;
	}

	// Find best output format for us
	output_format =
		avcodec_find_best_pix_fmt_of_list(supported_list, this->CodecCtx->pix_fmt, 1, NULL);

	// Set format configs
	Width = this->CodecCtx->width;
	Height = this->CodecCtx->height;
	Format = FindSDLPixelFormat(output_format);

	// Create scaler for handling format changes
	this->SWS = sws_getContext(this->CodecCtx->width, // Source w
		this->CodecCtx->height, // Source h
		this->CodecCtx->pix_fmt, // Source fmt
		this->CodecCtx->width, // Target w
		this->CodecCtx->height, // Target h
		FindAVPixelFormat(Format), // Target fmt
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL);
	if (this->SWS == NULL) {
		Log::Print(Log::LOG_ERROR, "Unable to initialize video converter context");
		exit(-1);
		goto exit_3;
	}

	// Set callbacks and userdata, and we're go
	this->DecodeFunc = VideoDecoder::DecodeFunction;
	this->CloseFunc = VideoDecoder::CloseFunction;

	Successful = true;

	return;

exit_3:
	av_frame_free(&this->ScratchFrame);
exit_2:
	// free(video_dec);
	// exit_1:
	// Kit_CloseDecoder(dec);
	// exit_0:
	return;
}
void* VideoDecoder::CreateVideoPacket(AVFrame* frame, double pts) {
	VideoPacket* packet = (VideoPacket*)calloc(1, sizeof(VideoPacket));
	if (!packet) {
		Log::Print(Log::LOG_ERROR,
			"Something went horribly wrong. (Ran out of memory at VideoDecoder::CreateVideoPacket \"(VideoPacket*)calloc\")");
		exit(-1);
	}
	packet->frame = frame;
	packet->pts = pts;
	return packet;
}
void VideoDecoder::FreeVideoPacket(void* p) {
	VideoPacket* packet = (VideoPacket*)p;
	av_freep(&packet->frame->data[0]);
	av_frame_free(&packet->frame);
	free(packet);
}

// Unique format info functions
AVPixelFormat VideoDecoder::FindAVPixelFormat(Uint32 format) {
	switch (format) {
	case SDL_PIXELFORMAT_YV12:
		return AV_PIX_FMT_YUV420P;
	case SDL_PIXELFORMAT_YUY2:
		return AV_PIX_FMT_YUYV422;
	case SDL_PIXELFORMAT_UYVY:
		return AV_PIX_FMT_UYVY422;
	case SDL_PIXELFORMAT_NV12:
		return AV_PIX_FMT_NV12;
	case SDL_PIXELFORMAT_NV21:
		return AV_PIX_FMT_NV21;
	case SDL_PIXELFORMAT_ARGB32:
		return AV_PIX_FMT_ARGB;
	case SDL_PIXELFORMAT_RGBA32:
		return AV_PIX_FMT_RGBA;
	case SDL_PIXELFORMAT_BGR24:
		return AV_PIX_FMT_BGR24;
	case SDL_PIXELFORMAT_RGB24:
		return AV_PIX_FMT_RGB24;
	case SDL_PIXELFORMAT_RGB555:
		return AV_PIX_FMT_RGB555;
	case SDL_PIXELFORMAT_BGR555:
		return AV_PIX_FMT_BGR555;
	case SDL_PIXELFORMAT_RGB565:
		return AV_PIX_FMT_RGB565;
	case SDL_PIXELFORMAT_BGR565:
		return AV_PIX_FMT_BGR565;
	default:
		return AV_PIX_FMT_NONE;
	}
}
int VideoDecoder::FindSDLPixelFormat(AVPixelFormat fmt) {
	switch (fmt) {
	case AV_PIX_FMT_YUV420P:
		return SDL_PIXELFORMAT_YV12;
	case AV_PIX_FMT_YUYV422:
		return SDL_PIXELFORMAT_YUY2;
	case AV_PIX_FMT_UYVY422:
		return SDL_PIXELFORMAT_UYVY;
	case AV_PIX_FMT_NV12:
		return SDL_PIXELFORMAT_NV12;
	case AV_PIX_FMT_NV21:
		return SDL_PIXELFORMAT_NV21;
	default:
		return SDL_PIXELFORMAT_RGBA32;
	}
}

// Common format info functions
int VideoDecoder::GetOutputFormat(OutputFormat* output) {
	output->Format = Format;
	output->Width = Width;
	output->Height = Height;
	return 0;
}

// Unique decoding functions
void VideoDecoder::ReadVideo(void* ptr) {
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	VideoDecoder* self = (VideoDecoder*)ptr;
	AVFrame* out_frame = NULL;
	VideoPacket* out_packet = NULL;
	double pts;
	int ret = 0;

	while (!ret && self->CanWriteOutput()) {
		ret = avcodec_receive_frame(self->CodecCtx, self->ScratchFrame);
		if (!ret) {
			out_frame = av_frame_alloc();
			av_image_alloc(out_frame->data,
				out_frame->linesize,
				self->CodecCtx->width,
				self->CodecCtx->height,
				self->FindAVPixelFormat(self->Format),
				1);

			// Scale from source format to target format,
			// don't touch the size
			sws_scale(self->SWS,
				(const unsigned char* const*)self->ScratchFrame->data,
				self->ScratchFrame->linesize,
				0,
				self->CodecCtx->height,
				out_frame->data,
				out_frame->linesize);

			// Get presentation timestamp
			pts = self->ScratchFrame->best_effort_timestamp;
			pts *= av_q2d(self->FormatCtx->streams[self->StreamIndex]->time_base);

			// Lock, write to audio buffer, unlock
			out_packet = (VideoPacket*)self->CreateVideoPacket(out_frame, pts);
			self->WriteOutput(out_packet);
		}
	}
#endif
}
int VideoDecoder::DecodeFunction(void* ptr, AVPacket* in_packet) {
	if (in_packet == NULL) {
		return 0;
	}
	VideoDecoder* self = (VideoDecoder*)ptr;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	// Try to clear the buffer first. We might have too much
	// content in the ffmpeg buffer, so we want to clear it of
	// outgoing data if we can.
	VideoDecoder::ReadVideo(self);

	// Write packet to the decoder for handling.
	if (avcodec_send_packet(self->CodecCtx, in_packet) < 0) {
		return 1;
	}

	// Some input data was put in successfully, so try again to get
	// frames.
	VideoDecoder::ReadVideo(self);
#else
	AVFrame* out_frame = NULL;
	VideoPacket* out_packet = NULL;
	int frame_finished;
	int len;
	double pts;

	while (in_packet->size > 0) {
		len = avcodec_decode_video2(
			self->CodecCtx, self->ScratchFrame, &frame_finished, in_packet);
		if (len < 0) {
			return 0;
		}

		if (frame_finished) {
			// Target frame
			out_frame = av_frame_alloc();
			av_image_alloc(out_frame->data,
				out_frame->linesize,
				self->CodecCtx->width,
				self->CodecCtx->height,
				self->FindAVPixelFormat(self->Format),
				1);

			// Scale from source format to target format,
			// don't touch the size
			sws_scale(this->SWS,
				(const unsigned char* const*)this->ScratchFrame->data,
				this->ScratchFrame->linesize,
				0,
				this->CodecCtx->height,
				out_frame->data,
				out_frame->linesize);

// Get presentation timestamp
#ifndef FF_API_FRAME_GET_SET
			pts = av_frame_get_best_effort_timestamp(this->ScratchFrame);
#else
			pts = this->ScratchFrame->best_effort_timestamp;
#endif
			pts *= av_q2d(this->FormatCtx->streams[this->StreamIndex]->time_base);

			// Lock, write to audio buffer, unlock
			out_packet = (VideoPacket*)self->CreateVideoPacket(out_frame, pts);
			self->WriteOutput(out_packet);
		}
		in_packet->size -= len;
		in_packet->data += len;
	}
#endif
	return 0;
}
void VideoDecoder::CloseFunction(void* ptr) {
	VideoDecoder* self = (VideoDecoder*)ptr;
	if (self->ScratchFrame != NULL) {
		av_frame_free(&self->ScratchFrame);
	}
	if (self->SWS != NULL) {
		sws_freeContext(self->SWS);
	}
}

// Data functions
double VideoDecoder::GetPTS() {
	VideoPacket* packet = (VideoPacket*)PeekOutput();
	if (packet == NULL) {
		return -1.0;
	}
	return packet->pts;
}
int VideoDecoder::GetVideoDecoderData(Texture* texture) {
	double sync_ts = 0.0;
	Uint32 limit_rounds = 0;
	VideoPacket* packet = NULL;

	// First, peek the next packet. Make sure we have at least one
	// frame to read.
	packet = (VideoPacket*)PeekOutput();
	if (packet == NULL) {
		return 0;
	}

	sync_ts = MediaPlayerState::GetSystemTime() - ClockSync;

	// If packet should not yet be played, stop here and wait.
	if (packet->pts > sync_ts + KIT_VIDEO_SYNC_THRESHOLD) {
		return 0;
	}

	// If packet should have already been played, skip it and try
	// to find a better packet.
	limit_rounds = GetOutputLength();
	while (packet != NULL && packet->pts < sync_ts - KIT_VIDEO_SYNC_THRESHOLD &&
		--limit_rounds) {
		AdvanceOutput();
		FreeVideoPacket(packet);
		packet = (VideoPacket*)PeekOutput();
	}
	if (packet == NULL) {
		return 0;
	}
	// For video, we *try* to return a frame, even if we are out of
	// sync. It is better than not showing anything.
	switch (Format) {
	case SDL_PIXELFORMAT_YV12:
	case SDL_PIXELFORMAT_IYUV:
		Graphics::UpdateYUVTexture(texture,
			NULL,
			packet->frame->data[0],
			packet->frame->linesize[0],
			packet->frame->data[1],
			packet->frame->linesize[1],
			packet->frame->data[2],
			packet->frame->linesize[2]);
		break;
	default:
		Graphics::UpdateTexture(
			texture, NULL, packet->frame->data[0], packet->frame->linesize[0]);
		break;
	}

	// Advance buffer, and free the decoded frame.
	AdvanceOutput();
	ClockPos = packet->pts;
	FreeVideoPacket(packet);
	return 1;
}

#endif
