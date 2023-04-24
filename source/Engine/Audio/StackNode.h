#ifndef ENGINE_AUDIO_STACKNODE_H
#define ENGINE_AUDIO_STACKNODE_H

class ISound;

enum {
    MusicFade_None,
    MusicFade_Out,
    MusicFade_In
};

struct StackNode {
    ISound*  Audio = NULL;
    bool     Loop = false;
    Uint32   LoopPoint = 0;
    Uint8    Fading = MusicFade_None;
    double   FadeTimer = 1.0;
    double   FadeTimerMax = 1.0;
    bool     Paused = false;
    bool     Stopped = false;
    Uint32   Speed = 0x10000;
    float    Pan = 0.0f;
    float    Volume = 0.0f;
    void*    Origin = nullptr;
};

#endif /* ENGINE_AUDIO_STACKNODE_H */
