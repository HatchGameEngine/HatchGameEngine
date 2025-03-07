#ifndef ENGINE_RESOURCETYPES_SOUNDFORMATS_OGG_H
#define ENGINE_RESOURCETYPES_SOUNDFORMATS_OGG_H

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

class OGG : public SoundFormat {
private:
	// OGG Specific
	char Vorbis[0x800];

	static size_t StaticRead(void* mem, size_t size, size_t nmemb, void* ptr);
	static Sint32 StaticSeek(void* ptr, Sint64 offset, int whence);
	static Sint32 StaticCloseFree(void* ptr);
	static Sint32 StaticCloseNoFree(void* ptr);
	static long StaticTell(void* ptr);

public:
	static SoundFormat* Load(const char* filename);
	size_t SeekSample(int index);
	int LoadSamples(size_t count);
	int GetSamples(Uint8* buffer, size_t count, Sint32 loopIndex);
	virtual void LoadAllSamples();
	void Dispose();
};

#endif /* ENGINE_RESOURCETYPES_SOUNDFORMATS_OGG_H */
