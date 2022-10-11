#ifndef ENGINE_RESOURCETYPES_SOUNDFORMATS_SOUNDFORMAT_H
#define ENGINE_RESOURCETYPES_SOUNDFORMATS_SOUNDFORMAT_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class Stream;

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/IO/Stream.h>

class SoundFormat {
public:
    // Common
    Stream*        StreamPtr = NULL;
    SDL_AudioSpec  InputFormat;
    SDL_AudioSpec  OutputFormat;
    vector<Uint8*> Samples;
    size_t         SampleSize;
    size_t         SampleIndex = 0;
    int            TotalPossibleSamples;
    Uint8*         SampleBuffer = NULL;
    Uint8*         SampleBufferHead = NULL;
    Sint32         LoopIndex = -1;

    virtual int    LoadSamples(size_t count);
    virtual int    GetSamples(Uint8* buffer, size_t count);
    virtual size_t SeekSample(int index);
    virtual size_t TellSample();
    virtual double GetPosition();
    virtual double SetPosition(double seconds);
    virtual double GetDuration();
    virtual        ~SoundFormat();
    virtual void   Close();
    virtual void   Dispose();
protected:
    void        LoadFinish();
};

#endif /* ENGINE_RESOURCETYPES_SOUNDFORMATS_SOUNDFORMAT_H */
