#ifndef ENGINE_AUDIO_AUDIOPLAYBACK_H
#define ENGINE_AUDIO_AUDIOPLAYBACK_H

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

class AudioPlayback {
private:
	void CreateConversionStream(SDL_AudioSpec format);

public:
	Uint8* Buffer = NULL;
	Uint8* UnconvertedSampleBuffer = NULL;
	Uint32 BufferedSamples = 0;
	SDL_AudioSpec Format;
	size_t BytesPerSample = 0;
	size_t RequiredSamples = 0;
	size_t DeviceBytesPerSample = 0;
	SDL_AudioStream* ConversionStream = NULL;
	SoundFormat* SoundData = NULL;
	bool OwnsSoundData = false;
	Sint32 LoopIndex = -1;

	AudioPlayback(SDL_AudioSpec format,
		size_t requiredSamples,
		size_t audioBytesPerSample,
		size_t deviceBytesPerSample);
	void Change(SDL_AudioSpec format,
		size_t requiredSamples,
		size_t audioBytesPerSample,
		size_t deviceBytesPerSample);
	void Dispose();
	int RequestSamples(int samples, bool loop, int sample_to_loop_to);
	void Seek(int samples);
	~AudioPlayback();
};

#endif /* ENGINE_AUDIO_AUDIOPLAYBACK_H */
