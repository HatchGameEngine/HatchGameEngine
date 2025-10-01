#ifndef ENGINE_RESOURCETYPES_SOUNDFORMATS_WAV_H
#define ENGINE_RESOURCETYPES_SOUNDFORMATS_WAV_H

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

class WAV : public SoundFormat {
private:
	// WAV Specific
	int DataStart = 0;

public:
	static SoundFormat* Load(Stream* stream);
	int LoadSamples(size_t count);
	void Dispose();
};

#endif /* ENGINE_RESOURCETYPES_SOUNDFORMATS_WAV_H */
