#ifndef ENGINE_AUDIO_AUDIOMANAGER_H
#define ENGINE_AUDIO_AUDIOMANAGER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Application.h>
#include <Engine/Audio/StackNode.h>
#include <Engine/ResourceTypes/ISound.h>

class AudioManager {
public:
    static SDL_AudioDeviceID Device;
    static SDL_AudioSpec     DeviceFormat;
    static bool              AudioEnabled;
    static Uint8             BytesPerSample;
    static Uint8*            MixBuffer;
    static size_t            MixBufferSize;
    static deque<StackNode*> MusicStack;
    static StackNode*        SoundArray;
    static int               SoundArrayLength;
    static double            FadeOutTimer;
    static double            FadeOutTimerMax;
    static float             MasterVolume;
    static float             MusicVolume;
    static float             SoundVolume;
    static float             LowPassFilter;
    static Uint8*            AudioQueue;
    static size_t            AudioQueueSize;
    static size_t            AudioQueueMaxSize;
    enum {
    REQUEST_EOF = 0,
    REQUEST_ERROR = -1,
    REQUEST_CONVERTING = -2,
    }; 

    static void   CalculateCoeffs();
    static Sint16 ProcessSample(Sint16 inSample, int channel);
    static float  ProcessSampleFloat(float inSample, int channel);
    static void   Init();
    static void   ClampParams(float& pan, float& speed, float& volume);
    static void   SetSound(int channel, ISound* music);
    static void   SetSound(int channel, ISound* music, bool loop, int loopPoint, float pan, float speed, float volume);
    static void   PushMusic(ISound* music, bool loop, Uint32 lp, float pan, float speed, float volume);
    static void   PushMusicAt(ISound* music, double at, bool loop, Uint32 lp, float pan, float speed, float volume);
    static void   RemoveMusic(ISound* music);
    static bool   IsPlayingMusic();
    static bool   IsPlayingMusic(ISound* music);
    static void   ClearMusic();
    static void   FadeMusic(double seconds);
    static void   AlterMusic(float pan, float speed, float volume);
    static void   Lock();
    static void   Unlock();
    static bool   AudioIsPlaying(int channel);
    static void   AudioUnpause(int channel);
    static void   AudioPause(int channel);
    static void   AudioStop(int channel);
    static void   AudioUnpauseAll();
    static void   AudioPauseAll();
    static void   AudioStopAll();
    static void   MixAudioLR(Uint8* dest, Uint8* src, size_t len, float volumeL, float volumeR);
    static bool   AudioPlayMix(StackNode* audio, Uint8* stream, int len, float volume);
    static void   AudioCallback(void* data, Uint8* stream, int len);
    static void   Dispose();
};

#endif /* ENGINE_AUDIO_AUDIOMANAGER_H */
