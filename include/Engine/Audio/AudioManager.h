#ifndef ENGINE_AUDIO_AUDIOMANAGER_H
#define ENGINE_AUDIO_AUDIOMANAGER_H

#include <Engine/Application.h>
#include <Engine/Audio/AudioChannel.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/ResourceTypes/ISound.h>

namespace AudioManager {
//private:
	void UpdateChannelPlayer(AudioPlayback* playback, ISound* sound);
	bool HandleFading(AudioChannel* audio);

//public:
	extern SDL_AudioDeviceID Device;
	extern SDL_AudioSpec DeviceFormat;
	extern bool AudioEnabled;
	extern Uint8 BytesPerSample;
	extern Uint8* MixBuffer;
	extern size_t MixBufferSize;
	extern deque<AudioChannel*> MusicStack;
	extern AudioChannel* SoundArray;
	extern int SoundArrayLength;
	extern float MasterVolume;
	extern float MusicVolume;
	extern float SoundVolume;
	extern float LowPassFilter;
	extern Uint8* AudioQueue;
	extern size_t AudioQueueSize;
	extern size_t AudioQueueMaxSize;
	enum {
		REQUEST_EOF = 0,
		REQUEST_ERROR = -1,
		REQUEST_CONVERTING = -2,
	};

	void CalculateCoeffs();
	Sint16 ProcessSample(Sint16 inSample, int channel);
	float ProcessSampleFloat(float inSample, int channel);
	void Init();
	void ClampParams(float& pan, float& speed, float& volume);
	void SetSound(int channel, ISound* music);
	void SetSound(int channel,
		ISound* sound,
		bool loop,
		int loopPoint,
		float pan,
		float speed,
		float volume,
		void* origin);
	int PlaySound(ISound* music);
	int PlaySound(ISound* music,
		bool loop,
		int loopPoint,
		float pan,
		float speed,
		float volume,
		void* origin);
	void PushMusic(ISound* music,
		bool loop,
		Uint32 lp,
		float pan,
		float speed,
		float volume,
		double fadeInAfterFinished);
	void PushMusicAt(ISound* music,
		double at,
		bool loop,
		Uint32 lp,
		float pan,
		float speed,
		float volume,
		double fadeInAfterFinished);
	void RemoveMusic(ISound* music);
	bool IsPlayingMusic();
	bool IsPlayingMusic(ISound* music);
	void ClearMusic();
	void ClearSounds();
	void FadeOutMusic(double seconds);
	void AlterMusic(float pan, float speed, float volume);
	double GetMusicPosition(ISound* music);
	void Lock();
	void Unlock();
	int GetFreeChannel();
	void AlterChannel(int channel, float pan, float speed, float volume);
	bool AudioIsPlaying(int channel);
	bool AudioIsPlaying(ISound* audio);
	void AudioUnpause(int channel);
	void AudioUnpause(ISound* audio);
	void AudioPause(int channel);
	void AudioPause(ISound* audio);
	void AudioStop(int channel);
	void AudioStop(ISound* audio);
	void AudioRemove(ISound* audio);
	void AudioUnpauseAll();
	void AudioPauseAll();
	void AudioStopAll();
	bool IsOriginPlaying(void* origin, ISound* audio);
	void StopOriginSound(void* origin, ISound* audio);
	void StopAllOriginSounds(void* origin);
	void MixAudioLR(Uint8* dest, Uint8* src, size_t len, float volumeL, float volumeR);
	bool AudioPlayMix(AudioChannel* audio, Uint8* stream, int len, float volume);
	void AudioCallback(void* data, Uint8* stream, int len);
	void Dispose();
};

#endif /* ENGINE_AUDIO_AUDIOMANAGER_H */
