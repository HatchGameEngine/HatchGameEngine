#ifndef ENGINE_MEDIA_MEDIAPLAYER_H
#define ENGINE_MEDIA_MEDIAPLAYER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Media/Decoder.h>
#include <Engine/Media/MediaSource.h>
#include <Engine/Rendering/Texture.h>

class MediaPlayer {
public:
	enum {
		KIT_WAITING_TO_BE_PLAYABLE = 0, ///< Playback stopped or has not started yet.
		KIT_PLAYING, ///< Playback started & player is actively decoding.
		KIT_PAUSED, ///< Playback paused; player is actively decoding but no new data is given out.
		KIT_CLOSED, ///< Playback is stopped and player is closing.
		KIT_STOPPED,
	};
	Uint32 State;
	Decoder* Decoders[3];
	SDL_Thread* DecoderThread;
	SDL_mutex* DecoderLock;
	MediaSource* Source;
	double PauseStarted;
	double PausedPosition;
	double SeekStarted;
	Uint32 WaitState;

	static int DemuxAllStreams(MediaPlayer* player);
	static int RunAllDecoders(MediaPlayer* player);
	static int DecoderThreadFunc(void* ptr);
	static MediaPlayer* Create(MediaSource* src,
		int video_stream_index,
		int audio_stream_index,
		int subtitle_stream_index,
		int screen_w,
		int screen_h);
	void Close();
	void SetScreenSize(int w, int h);
	int GetVideoStream();
	int GetAudioStream();
	int GetSubtitleStream();
	void GetInfo(PlayerInfo* info);
	double GetDuration();
	double GetPosition();
	double GetBufferPosition();
	bool ManageWaiting();
	int GetVideoData(Texture* texture);
	int GetVideoDataForPaused(Texture* texture);
	int GetAudioData(unsigned char* buffer, int length);
	int GetSubtitleData(Texture* texture, SDL_Rect* sources, SDL_Rect* targets, int limit);
	void SetClockSync();
	void SetClockSyncOffset(double offset);
	void ChangeClockSync(double delta);
	void Play();
	void Stop();
	void Pause();
	int Seek(double seek_set);
	Uint32 GetPlayerState();
	bool IsInputEmpty();
	bool IsOutputEmpty();
	static Uint32 GetInputLength(MediaPlayer* player, int i);
	static Uint32 GetOutputLength(MediaPlayer* player, int i);
};

#endif /* ENGINE_MEDIA_MEDIAPLAYER_H */
