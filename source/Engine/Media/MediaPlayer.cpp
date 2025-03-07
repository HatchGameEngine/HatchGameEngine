#ifdef USING_LIBAV

#include <Engine/Media/Decoders/AudioDecoder.h>
#include <Engine/Media/Decoders/VideoDecoder.h>
#include <Engine/Media/MediaPlayer.h>
// #include <Engine/Media/Decoders/SubtitleDecoder.h>
#include <Engine/Media/Utils/MediaPlayerState.h>

#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>

enum DecoderIndex { KIT_VIDEO_DEC = 0, KIT_AUDIO_DEC, KIT_SUBTITLE_DEC, KIT_DEC_COUNT };
enum DecoderRunReturn {
	DECODER_RUN_OKAY = 0,
	DECODER_RUN_EOF = 1,
};
enum DemuxerReturn {
	DEMUXER_KEEP_READING = -1,
	DEMUXER_INPUT_FULL = 0,
	DEMUXER_NO_PACKET = 1,
};

/*

All of this is based on SDL_Kitchensink

*/

// Demux/decode running functions
int MediaPlayer::DemuxAllStreams(MediaPlayer* player) {
	// Return  0 if stream is good but nothing else to do for now.
	// Return -1 if there may still work to be done.
	// Return  1 if there was an error or stream end.
	if (!player) {
		Log::Print(Log::LOG_ERROR, "MediaPlayer::DemuxAllStreams: player == NULL");
		exit(-1);
	}
	if (!player->Source) {
		Log::Print(Log::LOG_ERROR, "No source!");
		exit(-1);
	}
	if (!player->Source->FormatCtx) {
		Log::Print(Log::LOG_ERROR, "No source format context!");
		exit(-1);
	}

	AVFormatContext* format_ctx = (AVFormatContext*)player->Source->FormatCtx;

	// If any buffer is full, just stop here for now.
	// Since we don't know what kind of data is going to come out
	// of av_read_frame, we really want to make sure we are
	// prepared for everything.
	for (int i = 0; i < KIT_DEC_COUNT; i++) {
		Decoder* dec = player->Decoders[i];
		if (dec == NULL) {
			continue;
		}

		if (!dec->CanWriteInput()) {
			return DEMUXER_INPUT_FULL;
		}
	}

	// Attempt to read frame. Just return here if it fails.
	int ret;
	AVPacket* packet = av_packet_alloc();
	if ((ret = av_read_frame(format_ctx, packet)) < 0) {
		av_packet_free(&packet);
		if (ret != AVERROR_EOF) {
			char errorstr[256];
			av_strerror(ret, errorstr, sizeof(errorstr));
			Log::Print(Log::LOG_ERROR, "ret: (%s)", errorstr);
		}
		// Log::Print(Log::LOG_INFO, "pos: %f / %f",
		// player->GetPosition(), player->GetDuration());
		return DEMUXER_NO_PACKET;
	}

	// Check if this is a packet we need to handle and pass it on
	for (int i = 0; i < KIT_DEC_COUNT; i++) {
		Decoder* dec = player->Decoders[i];
		if (dec == NULL) {
			continue;
		}

		if (dec->StreamIndex == packet->stream_index) {
			dec->WriteInput(packet);
			return DEMUXER_KEEP_READING;
		}
	}

	// We only get here if packet was not written to a decoder. IF
	// that is the case, disregard and free the packet here, since
	// packets normally get freed via decoders.
	av_packet_free(&packet);
	return DEMUXER_KEEP_READING;
}
int MediaPlayer::RunAllDecoders(MediaPlayer* player) {
	int got;
	bool has_room = true;

	do {
		// Keep reading/demuxing until input full
		while ((got = MediaPlayer::DemuxAllStreams(player)) == DEMUXER_KEEP_READING)
			;

		// If the demuxer cannot read any more packets,
		// AND all input packets were decoded to frames,
		// AND we've run out of our decoded frames, this means
		// we've KIT_STOPPED This is a problem if the actual
		// video hasn't reached the end
		if (got == DEMUXER_NO_PACKET && player->IsInputEmpty() && player->IsOutputEmpty()) {
			return DECODER_RUN_EOF;
		}

		// Run decoder functions until outputs are full or
		// inputs are empty.
		for (int i = 0; i < KIT_DEC_COUNT; i++) {
			if (player->Decoders[i]) {
				while (player->Decoders[i]->Run() == 1)
					;
			}
		}

		// If there is no room in any decoder input, just stop
		// here since it likely means that at least some
		// decoder output is full.
		for (int i = 0; i < KIT_DEC_COUNT; i++) {
			Decoder* dec = player->Decoders[i];
			if (dec == NULL) {
				continue;
			}

			if (!dec->CanWriteInput()) {
				has_room = false;
				break;
			}
		}
	} while (has_room);
	return DECODER_RUN_OKAY;
}
int MediaPlayer::DecoderThreadFunc(void* ptr) {
	MediaPlayer* player = (MediaPlayer*)ptr;
	bool is_running = true;
	bool is_playing = true;

	while (is_running) {
		if (player->State == KIT_CLOSED) {
			is_running = false;
			continue;
		}
		if (player->State == KIT_PLAYING) {
			is_playing = true;
		}

		while (is_running && is_playing) {
			// Grab the decoder lock, and run demuxer &
			// decoders for a bit.
			if (SDL_LockMutex(player->DecoderLock) == 0) {
				if (player->State == KIT_CLOSED) {
					is_running = false;
					goto end_block;
				}
				if (player->State == KIT_STOPPED) {
					is_playing = false;
					goto end_block;
				}

				switch (MediaPlayer::RunAllDecoders(player)) {
				case DECODER_RUN_OKAY:
					// Decoder is okay.
					break;
				case DECODER_RUN_EOF:
					// Demuxer has reached eof and
					// decoder has no space.
					player->State = KIT_STOPPED;
					goto end_block;
				default:
					break;
				}

			end_block:
				SDL_UnlockMutex(player->DecoderLock);
			}
			// Delay to make sure this thread does not hog
			// all cpu
			SDL_Delay(2);
		}
		// Just idle while waiting for work.
		SDL_Delay(25);
	}
	return 0;
}

// Lifecycle functions
MediaPlayer* MediaPlayer::Create(MediaSource* src,
	int video_stream_index,
	int audio_stream_index,
	int subtitle_stream_index,
	int screen_w,
	int screen_h) {
	if (!src) {
		Log::Print(Log::LOG_ERROR, "MediaPlayer::Create: src == NULL");
		exit(-1);
	}
	if (screen_w <= 0) {
		Log::Print(Log::LOG_ERROR, "MediaPlayer::Create: screen_w == %d", screen_w);
		exit(-1);
	}
	if (screen_h <= 0) {
		Log::Print(Log::LOG_ERROR, "MediaPlayer::Create: screen_h == %d", screen_h);
		exit(-1);
	}

	MediaPlayer* player;

	if (!MediaPlayerState::LibassHandle) {
		// #ifdef USE_DYNAMIC_LIBASS
		//     MediaPlayerState::AssSharedObjectHandle =
		//     SDL_LoadObject(DYNAMIC_LIBASS_NAME); if
		//     (MediaPlayerState::AssSharedObjectHandle ==
		//     NULL) {
		//         Log::Print(Log::LOG_ERROR, "Unable to load
		//         ASS library"); return NULL;
		//     }
		//     load_libass(MediaPlayerState::AssSharedObjectHandle);
		// #endif
		// MediaPlayerState::LibassHandle = ass_library_init();
	}

	if (video_stream_index < 0 && subtitle_stream_index >= 0) {
		Log::Print(Log::LOG_ERROR, "Subtitle stream selected without video stream");
		goto exit_0;
	}

	player = (MediaPlayer*)calloc(1, sizeof(MediaPlayer));
	if (player == NULL) {
		Log::Print(Log::LOG_ERROR, "Unable to allocate player");
		goto exit_0;
	}

	// Initialize video decoder
	if (video_stream_index >= 0) {
		player->Decoders[KIT_VIDEO_DEC] = new VideoDecoder(src, video_stream_index);
		if (player->Decoders[KIT_VIDEO_DEC] == NULL) {
			goto exit_2;
		}
		if (!player->Decoders[KIT_VIDEO_DEC]->Successful) {
			goto exit_2;
		}
	}

	// Initialize audio decoder
	if (audio_stream_index >= 0) {
		player->Decoders[KIT_AUDIO_DEC] = new AudioDecoder(src, audio_stream_index);
		if (player->Decoders[KIT_AUDIO_DEC] == NULL) {
			goto exit_2;
		}
		if (!player->Decoders[KIT_AUDIO_DEC]->Successful) {
			goto exit_2;
		}
	}

	// Initialize subtitle decoder.
	if (subtitle_stream_index >= 0) {
		// OutputFormat output;
		// ((VideoDecoder*)player->Decoders[KIT_VIDEO_DEC])->GetOutputFormat(&output);
		// player->Decoders[KIT_SUBTITLE_DEC] = new
		// SubtitleDecoder(src, subtitle_stream_index,
		// output.Width, output.Height, screen_w, screen_h); if
		// (player->Decoders[KIT_SUBTITLE_DEC] == NULL) {
		//     goto exit_2;
		// }
		// if (!player->Decoders[KIT_SUBTITLE_DEC]->Successful)
		// {
		//     goto exit_2;
		// }
	}

	// Decoder thread lock
	player->DecoderLock = SDL_CreateMutex();
	if (player->DecoderLock == NULL) {
		Log::Print(Log::LOG_ERROR,
			"Unable to create a decoder thread lock mutex: %s",
			SDL_GetError());
		goto exit_2;
	}

	// Set source
	player->Source = src;
	player->SeekStarted = MediaPlayerState::GetSystemTime();
	player->SetClockSync();

	// Decoder thread
	player->DecoderThread = SDL_CreateThread(
		MediaPlayer::DecoderThreadFunc, "MediaPlayer::DecoderThreadFunc", player);
	if (player->DecoderThread == NULL) {
		Log::Print(Log::LOG_ERROR, "Unable to create a decoder thread: %s", SDL_GetError());
		goto exit_3;
	}

	return player;

exit_3:
	SDL_DestroyMutex(player->DecoderLock);
exit_2:
	for (int i = 0; i < KIT_DEC_COUNT; i++) {
		if (!player->Decoders[i]) {
			continue;
		}

		switch (i) {
		case KIT_VIDEO_DEC: {
			VideoDecoder* dec = (VideoDecoder*)player->Decoders[i];
			delete dec;
			break;
		}
		case KIT_AUDIO_DEC: {
			AudioDecoder* dec = (AudioDecoder*)player->Decoders[i];
			delete dec;
			break;
		}
		case KIT_SUBTITLE_DEC: {
			// SubtitleDecoder* dec =
			// (SubtitleDecoder*)player->Decoders[i];
			// delete dec;
			break;
		}
		}
	}
	// exit_1:
	free(player);
exit_0:
	return NULL;
}
void MediaPlayer::Close() {
	// Kill the decoder thread and mutex
	if (SDL_LockMutex(this->DecoderLock) == 0) {
		this->State = KIT_CLOSED;
		SDL_UnlockMutex(this->DecoderLock);
	}
	SDL_WaitThread(this->DecoderThread, NULL);
	SDL_DestroyMutex(this->DecoderLock);

	// Shutdown decoders
	for (int i = 0; i < KIT_DEC_COUNT; i++) {
		if (!this->Decoders[i]) {
			continue;
		}

		switch (i) {
		case KIT_VIDEO_DEC: {
			VideoDecoder* dec = (VideoDecoder*)this->Decoders[i];
			delete dec;
			break;
		}
		case KIT_AUDIO_DEC: {
			AudioDecoder* dec = (AudioDecoder*)this->Decoders[i];
			delete dec;
			break;
		}
		case KIT_SUBTITLE_DEC: {
			// SubtitleDecoder* dec =
			// (SubtitleDecoder*)this->Decoders[i]; delete
			// dec;
			break;
		}
		}
	}

	// Free the player structure itself
	free(this);
}

// Info functions
void MediaPlayer::SetScreenSize(int w, int h) {
	// SubtitleDecoder* dec =
	// (SubtitleDecoder*)Decoders[KIT_SUBTITLE_DEC]; if (dec ==
	// NULL)
	//     return;
	// dec->SetSize(w, h);
}
int MediaPlayer::GetVideoStream() {
	if (!Decoders[KIT_VIDEO_DEC]) {
		return -1;
	}
	return Decoders[KIT_VIDEO_DEC]->GetStreamIndex();
}
int MediaPlayer::GetAudioStream() {
	if (!Decoders[KIT_AUDIO_DEC]) {
		return -1;
	}
	return Decoders[KIT_AUDIO_DEC]->GetStreamIndex();
}
int MediaPlayer::GetSubtitleStream() {
	if (!Decoders[KIT_SUBTITLE_DEC]) {
		return -1;
	}
	return Decoders[KIT_SUBTITLE_DEC]->GetStreamIndex();
}
void MediaPlayer::GetInfo(PlayerInfo* info) {
	if (!info) {
		Log::Print(Log::LOG_ERROR, "MediaPlayer::GetInfo: info == NULL");
		exit(-1);
	}

	PlayerStreamInfo* streams[3] = {&info->Video, &info->Audio, &info->Subtitle};
	for (int i = 0; i < KIT_DEC_COUNT; i++) {
		Decoder* dec = this->Decoders[i];
		if (!dec) {
			continue;
		}

		PlayerStreamInfo* stream = streams[i];
		dec->GetCodecInfo(&stream->Codec);
		switch (i) {
		case KIT_VIDEO_DEC: {
			VideoDecoder* dec = (VideoDecoder*)this->Decoders[i];
			dec->GetOutputFormat(&stream->Output);
			break;
		}
		case KIT_AUDIO_DEC: {
			AudioDecoder* dec = (AudioDecoder*)this->Decoders[i];
			dec->GetOutputFormat(&stream->Output);
			break;
		}
		case KIT_SUBTITLE_DEC: {
			// SubtitleDecoder* dec =
			// (SubtitleDecoder*)this->Decoders[i];
			// dec->GetOutputFormat(&stream->Output);
			break;
		}
		}
	}
}
double MediaPlayer::GetDuration() {
	AVFormatContext* fmt_ctx = (AVFormatContext*)this->Source->FormatCtx;
	return (fmt_ctx->duration / AV_TIME_BASE);
}
double MediaPlayer::GetPosition() {
	if (State != KIT_PLAYING) {
		return PausedPosition;
	}

	if (this->Decoders[KIT_VIDEO_DEC]) {
		PausedPosition = ((Decoder*)this->Decoders[KIT_VIDEO_DEC])->ClockPos;
		return PausedPosition;
	}
	if (this->Decoders[KIT_AUDIO_DEC]) {
		PausedPosition = ((Decoder*)this->Decoders[KIT_AUDIO_DEC])->ClockPos;
		return PausedPosition;
	}
	return 0;
}
double MediaPlayer::GetBufferPosition() {
	double maxPTS = 0.0;
	for (int i = 0; i < KIT_DEC_COUNT; i++) {
		Decoder* dec = this->Decoders[i];
		if (!dec) {
			continue;
		}

		if (SDL_LockMutex(dec->OutputLock) == 0) {
			int KIT_DEC_BUF_OUT = 1;
			dec->Buffer[KIT_DEC_BUF_OUT]->WithEachItemInBuffer(
				[](void* data, void* opaque) -> void {
					double* pts = (double*)opaque;
					double* packet = (double*)data;
					if (*pts < *packet) {
						*pts = *packet;
					}
				},
				&maxPTS);
			SDL_UnlockMutex(dec->OutputLock);
		}
	}
	return maxPTS;
}

// Data functions
bool MediaPlayer::ManageWaiting() {
	if (IsOutputEmpty()) {
		return true;
	}
	else {
		double precise_pts = -1.0;
		if (this->Decoders[KIT_VIDEO_DEC]) {
			precise_pts = ((VideoDecoder*)this->Decoders[KIT_VIDEO_DEC])->GetPTS();
		}
		else if (this->Decoders[KIT_AUDIO_DEC]) {
			precise_pts = ((AudioDecoder*)this->Decoders[KIT_AUDIO_DEC])->GetPTS();
		}

		// Wait until we get a frame
		if (precise_pts < 0.0) {
			return true;
		}

		switch (this->WaitState) {
		case KIT_PAUSED:
			this->State = KIT_PAUSED;
			this->SetClockSyncOffset(-precise_pts);
			this->PauseStarted = MediaPlayerState::GetSystemTime();
			break;
		case KIT_PLAYING:
			this->State = KIT_PLAYING;
			this->SetClockSyncOffset(-precise_pts);
			break;
		default:
			printf("Invalid WaitState: %d\n", WaitState);
			break;
		}
		this->WaitState = 0;
	}
	return false;
}
int MediaPlayer::GetVideoData(Texture* texture) {
	Decoder* dec = (Decoder*)Decoders[KIT_VIDEO_DEC];
	if (dec == NULL) {
		return 0;
	}

	// If paused or stopped, do nothing
	if (this->State == KIT_PAUSED) {
		return 0;
	}
	if (this->State == KIT_STOPPED) {
		return 0;
	}
	if (this->State == KIT_WAITING_TO_BE_PLAYABLE) {
		if (ManageWaiting()) {
			return 0;
		}
	}

	return ((VideoDecoder*)dec)->GetVideoDecoderData(texture);
}
int MediaPlayer::GetVideoDataForPaused(Texture* texture) {
	Decoder* dec = (Decoder*)Decoders[KIT_VIDEO_DEC];
	if (dec == NULL) {
		return 0;
	}

	return ((VideoDecoder*)dec)->GetVideoDecoderData(texture);
}
int MediaPlayer::GetAudioData(unsigned char* buffer, int length) {
	if (!buffer) {
		Log::Print(Log::LOG_ERROR, "MediaPlayer::GetAudioData: buffer == NULL");
		exit(-1);
	}

	Decoder* dec = (Decoder*)Decoders[KIT_AUDIO_DEC];
	if (dec == NULL) {
		return 0;
	}

	// If asked for nothing, don't return anything either :P
	if (length == 0) {
		return 0;
	}

	// If paused or stopped, do nothing
	if (this->State == KIT_PAUSED) {
		return 0;
	}
	if (this->State == KIT_STOPPED) {
		return 0;
	}
	if (this->State == KIT_WAITING_TO_BE_PLAYABLE) {
		if (ManageWaiting()) {
			return 0;
		}
	}

	return ((AudioDecoder*)dec)->GetAudioDecoderData(buffer, length);
}
int MediaPlayer::GetSubtitleData(Texture* texture,
	SDL_Rect* sources,
	SDL_Rect* targets,
	int limit) {
	/*
	// NOTE: All asserts need to be removed/replaced.
	assert(texture != NULL);
	assert(sources != NULL);
	assert(targets != NULL);
	assert(limit >= 0);

	SubtitleDecoder* sub_dec =
	(SubtitleDecoder*)Decoders[KIT_SUBTITLE_DEC]; VideoDecoder*
	video_dec  = (VideoDecoder*)Decoders[KIT_VIDEO_DEC]; if
	(sub_dec == NULL || video_dec == NULL) { return 0;
	}

	// If paused, just return the current items
	if (this->State == KIT_PAUSED) {
	    return sub_dec->GetInfo(texture, sources, targets, limit);
	}

	// If stopped, do nothing.
	if (this->State == KIT_STOPPED) {
	    return 0;
	}

	if (this->State == KIT_WAITING_TO_BE_PLAYABLE) {
	    if (ManageWaiting()) {
	        return 0;
	    }
	}

	// Refresh texture, then refresh rects and return number of
	items in the texture. sub_dec->GetTexture(texture,
	video_dec->ClockPos); return sub_dec->GetInfo(texture, sources,
	targets, limit);
	//*/
	return 0;
}

// Clock functions
void MediaPlayer::SetClockSync() {
	double sync = MediaPlayerState::GetSystemTime();
	for (int i = 0; i < KIT_DEC_COUNT; i++) {
		if (Decoders[i]) {
			Decoders[i]->SetClockSync(sync);
		}
	}
}
void MediaPlayer::SetClockSyncOffset(double offset) {
	double sync = MediaPlayerState::GetSystemTime();
	for (int i = 0; i < KIT_DEC_COUNT; i++) {
		if (Decoders[i]) {
			Decoders[i]->SetClockSync(sync + offset);
		}
	}
}
void MediaPlayer::ChangeClockSync(double delta) {
	for (int i = 0; i < KIT_DEC_COUNT; i++) {
		if (Decoders[i]) {
			Decoders[i]->ChangeClockSync(delta);
		}
	}
}

// State functions
void MediaPlayer::Play() {
	double tmp;
	switch (this->State) {
	case KIT_WAITING_TO_BE_PLAYABLE:
		this->WaitState = KIT_PLAYING;
		break;

	case KIT_PLAYING:
	case KIT_CLOSED:
		break;

	case KIT_PAUSED:
		tmp = MediaPlayerState::GetSystemTime() - this->PauseStarted;
		this->ChangeClockSync(tmp);
		this->State = KIT_PLAYING;
		break;

	case KIT_STOPPED:
		if (SDL_LockMutex(this->DecoderLock) == 0) {
			for (int i = 0; i < KIT_DEC_COUNT; i++) {
				Decoder* dec = this->Decoders[i];
				if (!dec) {
					continue;
				}

				dec->ClearBuffers();
			}
			SDL_UnlockMutex(this->DecoderLock);
		}
		// MediaPlayer::RunAllDecoders(this); // Fill some
		// buffers before starting playback
		this->SetClockSync();
		this->State = KIT_PLAYING;
		break;
	}
}
void MediaPlayer::Stop() {
	MediaPlayer* player = this;
	if (SDL_LockMutex(player->DecoderLock) == 0) {
		switch (player->State) {
		case KIT_STOPPED:
		case KIT_CLOSED:
			break;
		case KIT_PLAYING:
		case KIT_PAUSED:
			player->State = KIT_STOPPED;
			for (int i = 0; i < KIT_DEC_COUNT; i++) {
				if (player->Decoders[i]) {
					player->Decoders[i]->ClearBuffers();
				}
			}
			// printf("ClockSync: %f\n",
			// player->Decoders[0]->ClockSync);
			break;
		}
		SDL_UnlockMutex(player->DecoderLock);
	}
}
void MediaPlayer::Pause() {
	switch (this->State) {
	case KIT_WAITING_TO_BE_PLAYABLE:
		this->WaitState = KIT_PAUSED;
		return;
	default:
		break;
	}

	this->State = KIT_PAUSED;
	this->PauseStarted = MediaPlayerState::GetSystemTime();
}
int MediaPlayer::Seek(double seek_set) {
	MediaPlayer* player = this;
	double position;
	double duration = 1.0;
	int64_t seek_target;
	int flags = AVSEEK_FLAG_ANY;

	SeekStarted = MediaPlayerState::GetSystemTime();

	if (SDL_LockMutex(player->DecoderLock) == 0) {
		position = player->GetPosition();
		duration = player->GetDuration();
		if (seek_set <= 0) {
			seek_set = 0;
		}
		if (seek_set >= duration) {
			seek_set = duration;
			// Just do nothing if trying to skip to the end
			SDL_UnlockMutex(player->DecoderLock);
			return 0;
		}

		// Set source to timestamp
		AVFormatContext* format_ctx = (AVFormatContext*)player->Source->FormatCtx;
		seek_target = seek_set * AV_TIME_BASE;
		if (seek_set < position) {
			flags |= AVSEEK_FLAG_BACKWARD;
		}

		// First, tell ffmpeg to seek stream. If not capable,
		// stop here. Failure here probably means that stream
		// is unseekable someway, eg. streamed media NOTE: this
		// sets the read position of the stream: if we don't
		// need it, we should skip this
		//    if the desired position is already loaded.
		int stream_index = -1;
		if (player->Decoders[KIT_AUDIO_DEC]) {
			stream_index = player->Decoders[KIT_AUDIO_DEC]->GetStreamIndex();
		}

		if (av_seek_frame(format_ctx, stream_index, seek_target, flags) < 0) {
			// if (avformat_seek_file(format_ctx,
			// stream_index, seek_target, seek_target,
			// seek_target, flags) < 0) {
			Log::Print(Log::LOG_ERROR, "Unable to seek source");
			SDL_UnlockMutex(player->DecoderLock);
			return 1;
		}

		printf("seeking to: %f; from: %f ---> %f\n",
			seek_set,
			position,
			(double)format_ctx->pb->pos / AV_TIME_BASE);

		bool seekTargetWithinOutputFrames = false;
		if (seekTargetWithinOutputFrames) {
			// Just set the ClockSync, frames will catch up
			player->ChangeClockSync(seek_set - position);
		}
		else {
			// Clean old buffers and try to fill them with
			// new data
			for (int i = 0; i < KIT_DEC_COUNT; i++) {
				if (player->Decoders[i]) {
					player->Decoders[i]->ClearBuffers();
				}
			}

			if (player->State != KIT_WAITING_TO_BE_PLAYABLE) {
				player->WaitState = player->State;
				player->State = KIT_WAITING_TO_BE_PLAYABLE;
			}
		}

		PausedPosition = seek_set;

		// That's it. Unlock and continue.
		SDL_UnlockMutex(player->DecoderLock);
	}

	return 0;
}
Uint32 MediaPlayer::GetPlayerState() {
	return this->State;
}

// Buffer checks
bool MediaPlayer::IsInputEmpty() {
	for (int i = 0; i < KIT_DEC_COUNT; i++) {
		Decoder* dec = (Decoder*)this->Decoders[i];
		if (dec == NULL) {
			continue;
		}
		if (dec->PeekInput()) {
			return false;
		}
	}
	return true;
}
bool MediaPlayer::IsOutputEmpty() {
	for (int i = 0; i < KIT_DEC_COUNT; i++) {
		Decoder* dec = (Decoder*)this->Decoders[i];
		if (dec == NULL) {
			continue;
		}
		if (dec->PeekOutput()) {
			return false;
		}
	}
	return true;
}

// ???
Uint32 MediaPlayer::GetInputLength(MediaPlayer* player, int i) {
	Decoder* dec = player->Decoders[i];
	if (dec == NULL) {
		return 0;
	}
	return dec->GetInputLength();
}
Uint32 MediaPlayer::GetOutputLength(MediaPlayer* player, int i) {
	Decoder* dec = player->Decoders[i];
	if (dec == NULL) {
		return 0;
	}
	return dec->GetOutputLength();
}

#elif 0

int MediaPlayer::DemuxAllStreams(MediaPlayer* player) {
	return DEMUXER_KEEP_READING;
}
int MediaPlayer::RunAllDecoders(MediaPlayer* player) {
	return DECODER_RUN_OKAY;
}
int MediaPlayer::DecoderThreadFunc(void* ptr) {
	return 0;
}

MediaPlayer* MediaPlayer::Create(MediaSource* src,
	int video_stream_index,
	int audio_stream_index,
	int subtitle_stream_index,
	int screen_w,
	int screen_h) {
	return NULL;
}
void MediaPlayer::Close() {}

void MediaPlayer::SetScreenSize(int w, int h) {}
int MediaPlayer::GetVideoStream() {
	return 0;
}
int MediaPlayer::GetAudioStream() {
	return 0;
}
int MediaPlayer::GetSubtitleStream() {
	return 0;
}
void MediaPlayer::GetInfo(PlayerInfo* info) {}
double MediaPlayer::GetDuration() {
	return 0.0;
}
double MediaPlayer::GetPosition() {
	return 0.0;
}
double MediaPlayer::GetBufferPosition() {
	return 0.0;
}

bool MediaPlayer::ManageWaiting() {
	return false;
}
int MediaPlayer::GetVideoData(Texture* texture) {
	return 0;
}
int MediaPlayer::GetVideoDataForPaused(Texture* texture) {
	return 0;
}
int MediaPlayer::GetAudioData(unsigned char* buffer, int length) {
	return 0;
}
int MediaPlayer::GetSubtitleData(Texture* texture,
	SDL_Rect* sources,
	SDL_Rect* targets,
	int limit) {
	return 0;
}

void MediaPlayer::SetClockSync() {}
void MediaPlayer::SetClockSyncOffset(double offset) {}
void MediaPlayer::ChangeClockSync(double delta) {}

void MediaPlayer::Play() {}
void MediaPlayer::Stop() {}
void MediaPlayer::Pause() {}
int MediaPlayer::Seek(double seek_set) {
	return 0;
}
Uint32 MediaPlayer::GetPlayerState() {
	return this->State;
}

bool MediaPlayer::IsInputEmpty() {
	return true;
}
bool MediaPlayer::IsOutputEmpty() {
	return true;
}

Uint32 MediaPlayer::GetInputLength(MediaPlayer* player, int i) {
	return 0;
}
Uint32 MediaPlayer::GetOutputLength(MediaPlayer* player, int i) {
	return 0;
}
#endif
