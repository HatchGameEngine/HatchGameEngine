#ifndef ENGINE_RESOURCETYPES_SOUNDFORMATS_SOUNDFORMAT_H
#define ENGINE_RESOURCETYPES_SOUNDFORMATS_SOUNDFORMAT_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>

class SoundFormat {
public:
	// Common
	Stream* StreamPtr = NULL;
	SDL_AudioSpec InputFormat;
	vector<Uint8*> Samples;
	size_t SampleSize;
	size_t SampleIndex = 0;
	int TotalPossibleSamples;
	Uint8* SampleBuffer = NULL;

	virtual int LoadSamples(size_t count);
	virtual int GetSamples(Uint8* buffer, size_t count, Sint32 loopIndex);
	virtual size_t SeekSample(int index);
	virtual size_t TellSample();
	virtual void LoadAllSamples();
	void CopySamples(SoundFormat* dest);
	virtual double GetPosition();
	virtual double SetPosition(double seconds);
	virtual double GetDuration();
	virtual ~SoundFormat();
	virtual void Close();
	virtual void Dispose();

protected:
	void LoadFinish();
};

#endif /* ENGINE_RESOURCETYPES_SOUNDFORMATS_SOUNDFORMAT_H */
