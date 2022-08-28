#ifndef INPUT_H
#define INPUT_H

enum class ControllerType {
    Unknown,
    Xbox360,
    XboxOne,
    XboxSeriesXS,
    XboxElite,
    PS3,
    PS4,
    PS5,
    SwitchJoyConPair,
    SwitchJoyConLeft,
    SwitchJoyConRight,
    SwitchPro,
    Stadia,
    AmazonLuna,
    NvidiaShield
};

enum class ControllerButton {
    A,
    B,
    X,
    Y,
    Back,
    Guide,
    Start,
    LeftStick,
    RightStick,
    LeftShoulder,
    RightShoulder,
    DPadUp,
    DPadDown,
    DPadLeft,
    DPadRight,
    Share,
    Microphone,
    Touchpad,
    Paddle1,
    Paddle2,
    Paddle3,
    Paddle4,
    Misc1,
    Max
};

enum class ControllerAxis {
    LeftX,
    LeftY,
    RightX,
    RightY,
    TriggerLeft,
    TriggerRight,
    Max
};

struct ControllerRumble {
    float  LargeMotorFrequency;
    float  SmallMotorFrequency;
    Uint32 TicksLeft;
    Uint32 Expiration;
    bool   Active;
    bool   Paused;
    void*  Device;

    ControllerRumble(void *device) {
        Active = false;
        LargeMotorFrequency = SmallMotorFrequency = 0;
        TicksLeft = Expiration = 0;
        Device = device;
    };
    bool Enable(float large_frequency, float small_frequency, Uint32 duration) {
        if (large_frequency < 0.0f)
            large_frequency = 0.0f;
        else if (large_frequency > 1.0f)
            large_frequency = 1.0f;

        if (small_frequency < 0.0f)
            small_frequency = 0.0f;
        else if (small_frequency > 1.0f)
            small_frequency = 1.0f;

        Uint16 largeMotorFrequency = large_frequency * 0xFFFF;
        Uint16 smallMotorFrequency = small_frequency * 0xFFFF;

        if (SDL_GameControllerRumble((SDL_GameController*)Device, large_frequency, small_frequency, 0) == -1)
            return false;

        Active = true;
        LargeMotorFrequency = large_frequency;
        SmallMotorFrequency = small_frequency;

        if (duration)
            Expiration = SDL_GetTicks() + duration;
        else
            Expiration = 0;

        return true;
    };
    bool SetLargeMotorFrequency(float frequency) {
        if (frequency < 0.0f)
            frequency = 0.0f;
        else if (frequency > 1.0f)
            frequency = 1.0f;

        Uint16 largeMotorFrequency = frequency * 0xFFFF;
        if (SDL_GameControllerRumble((SDL_GameController*)Device, largeMotorFrequency, SmallMotorFrequency, 0) == -1)
            return false;

        Active = true;
        LargeMotorFrequency = frequency;

        return true;
    };
    bool SetSmallMotorFrequency(float frequency) {
        if (frequency < 0.0f)
            frequency = 0.0f;
        else if (frequency > 1.0f)
            frequency = 1.0f;

        Uint16 smallMotorFrequency = frequency * 0xFFFF;
        if (SDL_GameControllerRumble((SDL_GameController*)Device, LargeMotorFrequency, smallMotorFrequency, 0) == -1)
            return false;

        Active = true;
        SmallMotorFrequency = frequency;

        return true;
    };
    void Update() {
        if (!Active)
            return;

        if (Expiration && !Paused && SDL_TICKS_PASSED(SDL_GetTicks(), Expiration))
            Stop();
    };
    void Stop() {
        Active = false;
        LargeMotorFrequency = SmallMotorFrequency = 0;
        TicksLeft = Expiration = 0;
        SDL_GameControllerRumble((SDL_GameController*)Device, 0, 0, 0);
    };
    void SetPaused(bool paused) {
        if (!Active || paused == Paused)
            return;

        Paused = paused;

        if (Paused) {
            SDL_GameControllerRumble((SDL_GameController*)Device, 0, 0, 0);

            if (Expiration) {
                TicksLeft = Expiration - SDL_GetTicks();
                Expiration = 0;
            }
        }
        else {
            Uint16 largeMotorFrequency = LargeMotorFrequency * 0xFFFF;
            Uint16 smallMotorFrequency = SmallMotorFrequency * 0xFFFF;
            SDL_GameControllerRumble((SDL_GameController*)Device, largeMotorFrequency, smallMotorFrequency, 0);

            if (TicksLeft)
                Expiration = SDL_GetTicks() + TicksLeft;
        }
    };
};

#endif /* INPUT_H */
