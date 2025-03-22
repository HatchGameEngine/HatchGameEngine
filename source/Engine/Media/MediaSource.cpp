#include <Engine/Media/MediaSource.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Media/Includes/AVUtils.h>

#ifdef USING_LIBAV

int _ReadPacket(void* opaque, Uint8* buf, int buf_size) {
	Stream* stream = (Stream*)opaque;
	size_t read = stream->ReadBytes(buf, buf_size);
	if (!read) {
		return AVERROR_EOF;
	}

	return (int)read;
}
int64_t _SeekPacket(void* opaque, int64_t offset, int whence) {
	Stream* stream = (Stream*)opaque;
	switch (whence) {
	case SEEK_SET:
		stream->Seek(offset);
		break;
	case SEEK_CUR:
		stream->Skip(offset);
		break;
	case SEEK_END:
		stream->SeekEnd(offset);
		break;
	case AVSEEK_SIZE:
		return (int64_t)stream->Length();
		break;
	}
	return (int64_t)stream->Position();
}
void _AVLogCallback(void* ptr, int level, const char* fmt, va_list vargs) {
	if (!fmt) {
		return;
	}

	int fin = Log::LOG_VERBOSE;
	switch (level) {
	case AV_LOG_INFO:
		fin = Log::LOG_INFO;
		break;
	case AV_LOG_ERROR:
		fin = Log::LOG_ERROR;
		break;
	case AV_LOG_WARNING:
		fin = Log::LOG_WARN;
		break;
	case AV_LOG_FATAL:
	case AV_LOG_PANIC:
		fin = Log::LOG_IMPORTANT;
		break;
	default:
		return;
	}

	char str[MAX_PATH_LENGTH];
	vsnprintf(str, sizeof str, fmt, vargs);
	str[strlen(str) - 1] = 0;

	char* force = (char*)str + strlen(str);
	for (; force >= str; force--) {
		if (*force == '\n') {
			*force = ' ';
			break;
		}
	}
	Log::Print(fin, str);
}

#endif

MediaSource* MediaSource::CreateSourceFromUrl(const char* url) {
	if (!url) {
		return NULL;
	}

	MediaSource* src = (MediaSource*)Memory::TrackedCalloc(
		"MediaSource::MediaSource", 1, sizeof(MediaSource));
	if (!src) {
		Log::Print(Log::LOG_ERROR, "Unable to allocate MediaSource!");
		return NULL;
	}

#ifdef USING_LIBAV
	if (avformat_open_input((AVFormatContext**)&src->FormatCtx, url, NULL, NULL) < 0) {
		Log::Print(Log::LOG_ERROR, "Unable to open source URL: %s!", url);
		goto __FREE;
	}

	av_opt_set_int((AVFormatContext*)src->FormatCtx, "probesize", INT_MAX, 0);
	av_opt_set_int((AVFormatContext*)src->FormatCtx, "analyzeduration", INT_MAX, 0);
	if (avformat_find_stream_info((AVFormatContext*)src->FormatCtx, NULL) < 0) {
		Log::Print(Log::LOG_ERROR, "Unable to fetch source info!");
		goto __CLOSE;
	}

	return src;

__CLOSE:
	avformat_close_input((AVFormatContext**)&src->FormatCtx);

__FREE:
#endif
	Memory::Free(src);

	return NULL;
}
MediaSource* MediaSource::CreateSourceFromStream(Stream* stream) {
#ifdef USING_LIBAV
	AVIOContext* avio_ctx = NULL;
	Uint8* avio_ctx_buffer = NULL;
	size_t avio_ctx_buffer_size = 4096; // Typical cache page size (4KB)

	av_log_set_level(AV_LOG_ERROR);
	av_log_set_callback(_AVLogCallback);

	MediaSource* src = (MediaSource*)Memory::TrackedCalloc(
		"MediaSource::MediaSource", 1, sizeof(MediaSource));
	if (!src) {
		Log::Print(Log::LOG_ERROR, "Unable to allocate MediaSource!");
		return NULL;
	}

	src->StreamPtr = stream;

	if (!(src->FormatCtx = avformat_alloc_context())) {
		Log::Print(Log::LOG_ERROR, "Unable to allocate Format context!");
		goto __FREE;
	}
	avio_ctx_buffer = (Uint8*)av_malloc(avio_ctx_buffer_size);
	if (!avio_ctx_buffer) {
		Log::Print(Log::LOG_ERROR, "Unable to allocate AVIO context buffer!");
		goto __CLOSE;
	}
	avio_ctx = avio_alloc_context(avio_ctx_buffer,
		avio_ctx_buffer_size,
		false, // isWritable
		stream, // opaque pointer
		&_ReadPacket, // int(*)(void *opaque, uint8_t *buf, int
		// buf_size) 	read_packet,
		NULL, // int(*)(void *opaque, uint8_t *buf, int
		// buf_size) 	write_packet,
		&_SeekPacket); // int64_t(*)(void *opaque, int64_t
	// offset, int whence) 	seek
	if (!avio_ctx) {
		// ret = AVERROR(ENOMEM);
		Log::Print(Log::LOG_ERROR, "Unable to allocate AVIO context!");
		goto __CLOSE;
	}
	((AVFormatContext*)src->FormatCtx)->pb = avio_ctx;
	((AVFormatContext*)src->FormatCtx)->flags |= AVFMT_FLAG_CUSTOM_IO;
	// ((AVFormatContext*)src->FormatCtx)->flags |=
	// AVFMT_FLAG_FAST_SEEK;
	// ((AVFormatContext*)src->FormatCtx)->flags |=
	// AVFMT_SEEK_TO_PTS;

	if (avformat_open_input((AVFormatContext**)&src->FormatCtx, NULL, NULL, NULL) < 0) {
		Log::Print(Log::LOG_ERROR, "Unable to open source!");
		goto __FREE;
	}

	av_opt_set_int((AVFormatContext*)src->FormatCtx, "probesize", INT_MAX, 0);
	av_opt_set_int((AVFormatContext*)src->FormatCtx, "analyzeduration", INT_MAX, 0);
	if (avformat_find_stream_info((AVFormatContext*)src->FormatCtx, NULL) < 0) {
		Log::Print(Log::LOG_ERROR, "Unable to fetch source info!");
		goto __CLOSE;
	}

	return src;

__CLOSE:
	avformat_close_input((AVFormatContext**)&src->FormatCtx);

	if (avio_ctx_buffer) {
		av_freep(avio_ctx_buffer);
	}
	if (avio_ctx) {
		av_freep(&avio_ctx->buffer);
	}
	avio_context_free(&avio_ctx);

__FREE:
	Memory::Free(src);
#endif
	stream->Close();

	return NULL;
}

int MediaSource::GetStreamInfo(Uint32* info, int index) {
	if (!info) {
		return 1;
	}

#ifdef USING_LIBAV

	AVFormatContext* format_ctx = (AVFormatContext*)this->FormatCtx;
	if (index < 0 || (Uint32)index >= format_ctx->nb_streams) {
		Log::Print(Log::LOG_ERROR, "Invalid stream index");
		return 1;
	}

	AVStream* stream = format_ctx->streams[index];
	enum AVMediaType codec_type;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 48, 101)
	codec_type = stream->codec->codec_type;
#else
	codec_type = stream->codecpar->codec_type;
#endif

	switch (codec_type) {
	case AVMEDIA_TYPE_UNKNOWN:
		*info = STREAMTYPE_UNKNOWN;
		break;
	case AVMEDIA_TYPE_DATA:
		*info = STREAMTYPE_DATA;
		break;
	case AVMEDIA_TYPE_VIDEO:
		*info = STREAMTYPE_VIDEO;
		break;
	case AVMEDIA_TYPE_AUDIO:
		*info = STREAMTYPE_AUDIO;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		*info = STREAMTYPE_SUBTITLE;
		break;
	case AVMEDIA_TYPE_ATTACHMENT:
		*info = STREAMTYPE_ATTACHMENT;
		break;
	default:
		Log::Print(Log::LOG_ERROR, "Unknown native stream type");
		return 1;
	}

#endif

	return 0;
}
int MediaSource::GetStreamCount() {
#ifdef USING_LIBAV
	return ((AVFormatContext*)FormatCtx)->nb_streams;
#endif
	return 0;
}
int MediaSource::GetBestStream(Uint32 type) {
#ifdef USING_LIBAV
	int avmedia_type = 0;
	switch (type) {
	case STREAMTYPE_VIDEO:
		avmedia_type = AVMEDIA_TYPE_VIDEO;
		break;
	case STREAMTYPE_AUDIO:
		avmedia_type = AVMEDIA_TYPE_AUDIO;
		break;
	case STREAMTYPE_SUBTITLE:
		avmedia_type = AVMEDIA_TYPE_SUBTITLE;
		break;
	default:
		return -1;
	}
	int ret = av_find_best_stream(
		(AVFormatContext*)this->FormatCtx, (enum AVMediaType)avmedia_type, -1, -1, NULL, 0);
	if (ret == AVERROR_STREAM_NOT_FOUND) {
		return -1;
	}
	if (ret == AVERROR_DECODER_NOT_FOUND) {
		Log::Print(Log::LOG_ERROR, "Unable to find a decoder for the stream");
		return 1;
	}
	return ret;
#endif
	return -1;
}

void MediaSource::Close() {
#ifdef USING_LIBAV
	AVFormatContext* format_ctx = (AVFormatContext*)this->FormatCtx;
	AVIOContext* avio_ctx = (AVIOContext*)this->AvioCtx;
	avformat_close_input(&format_ctx);
	if (avio_ctx) {
		av_freep(&avio_ctx->buffer);
		av_freep(&avio_ctx);
	}
#endif
	StreamPtr->Close();
	Memory::Free(this);
}
