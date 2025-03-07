#ifndef ENGINE_RESOURCETYPES_ISOUND_H
#define ENGINE_RESOURCETYPES_ISOUND_H

#include <Engine/Application.h>
#include <Engine/Audio/AudioPlayback.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

class ISound {
public:
	SDL_AudioSpec Format;
	int BytesPerSample;
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
