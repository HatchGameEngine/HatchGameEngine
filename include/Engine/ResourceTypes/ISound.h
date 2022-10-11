#ifndef ENGINE_RESOURCETYPES_ISOUND_H
#define ENGINE_RESOURCETYPES_ISOUND_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED

class SoundFormat;

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Application.h>
#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

class ISound {
public:
    SDL_AudioSpec     Format;
    int               BytesPerSample;
    Uint8*            Buffer = NULL;
    Uint8*            UnconvertedSampleBuffer = NULL;
    Uint32            BufferedSamples = 0;
    char              Filename[256];
    SDL_AudioStream*  Stream = NULL;
    bool              LoadFailed = false;
    bool              StreamFromFile = false;
    SoundFormat*      SoundData = NULL;

         ISound(const char* filename);
         ISound(const char* filename, bool streamFromFile);
    void Load(const char* filename, bool streamFromFile);
    int  RequestSamples(int samples, bool loop, int sample_to_loop_to);
    void Seek(int samples);
    void Dispose();
};

#endif /* ENGINE_RESOURCETYPES_ISOUND_H */
