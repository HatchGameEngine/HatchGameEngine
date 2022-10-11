#ifndef ENGINE_AUDIO_STACKNODE_H
#define ENGINE_AUDIO_STACKNODE_H

class ISound;

struct StackNode {
    ISound*  Audio = NULL;
    bool     Loop = false;
    Uint32   LoopPoint = 0;
    bool     FadeOut = false;
    bool     Paused = false;
    bool     Stopped = false;
    Uint32   Speed = 0x10000;
    float    Pan = 0.0f;
    float    Volume = 0.0f;
};

#endif /* ENGINE_AUDIO_STACKNODE_H */
