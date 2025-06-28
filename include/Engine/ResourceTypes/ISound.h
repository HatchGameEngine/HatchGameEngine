#ifndef ENGINE_RESOURCETYPES_ISOUND_H
#define ENGINE_RESOURCETYPES_ISOUND_H

#include <Engine/Application.h>
#include <Engine/Audio/AudioPlayback.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

#define AUDIO_LOOP_NONE (-2)
#define AUDIO_LOOP_DEFAULT (-1)

class ISound {
public:
	SDL_AudioSpec Format;
	int BytesPerSample;
	int LoopPoint = AUDIO_LOOP_NONE;
	SoundFormat* SoundData = NULL;
	char* Filename = NULL;
	bool LoadFailed = false;
	bool StreamFromFile = false;

	ISound(const char* filename);
	ISound(const char* filename, bool streamFromFile);
	void Load(const char* filename, bool streamFromFile);
	AudioPlayback* CreatePlayer();
	void Dispose();
};

#endif /* ENGINE_RESOURCETYPES_ISOUND_H */
