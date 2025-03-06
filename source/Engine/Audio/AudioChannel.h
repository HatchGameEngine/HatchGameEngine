#ifndef ENGINE_AUDIO_AUDIOCHANNEL_H
#define ENGINE_AUDIO_AUDIOCHANNEL_H

class ISound;

#include <Engine/Audio/AudioIncludes.h>
#include <Engine/Audio/AudioPlayback.h>

struct AudioChannel {
	ISound* Audio = nullptr;
	AudioPlayback* Playback = nullptr;
	bool Loop = false;
	Uint32 LoopPoint = 0;
	Uint8 Fading = MusicFade_None;
	double FadeTimer = 1.0;
	double FadeTimerMax = 1.0;
	bool Paused = false;
	bool Stopped = false;
	Uint32 Speed = 0x10000;
	float Pan = 0.0f;
	float Volume = 0.0f;
	void* Origin = nullptr;

	~AudioChannel() {
		delete Playback;
		Playback = nullptr;
	}
};

#endif /* ENGINE_AUDIO_AUDIOCHANNEL_H */
