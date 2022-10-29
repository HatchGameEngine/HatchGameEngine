#ifndef INPUT_H
#define INPUT_H

enum Keyboard {
    Key_A,
    Key_B,
    Key_C,
    Key_D,
    Key_E,
    Key_F,
    Key_G,
    Key_H,
    Key_I,
    Key_J,
    Key_K,
    Key_L,
    Key_M,
    Key_N,
    Key_O,
    Key_P,
    Key_Q,
    Key_R,
    Key_S,
    Key_T,
    Key_U,
    Key_V,
    Key_W,
    Key_X,
    Key_Y,
    Key_Z,

    Key_1,
    Key_2,
    Key_3,
    Key_4,
    Key_5,
    Key_6,
    Key_7,
    Key_8,
    Key_9,
    Key_0,

    Key_RETURN,
    Key_ESCAPE,
    Key_BACKSPACE,
    Key_TAB,
    Key_SPACE,

    Key_MINUS,
    Key_EQUALS,
    Key_LEFTBRACKET,
    Key_RIGHTBRACKET,
    Key_BACKSLASH,
    Key_SEMICOLON,
    Key_APOSTROPHE,
    Key_GRAVE,
    Key_COMMA,
    Key_PERIOD,
    Key_SLASH,

    Key_CAPSLOCK,

    Key_F1,
    Key_F2,
    Key_F3,
    Key_F4,
    Key_F5,
    Key_F6,
    Key_F7,
    Key_F8,
    Key_F9,
    Key_F10,
    Key_F11,
    Key_F12,

    Key_PRINTSCREEN,
    Key_SCROLLLOCK,
    Key_PAUSE,
    Key_INSERT,
    Key_HOME,
    Key_PAGEUP,
    Key_DELETE,
    Key_END,
    Key_PAGEDOWN,
    Key_RIGHT,
    Key_LEFT,
    Key_DOWN,
    Key_UP,

    Key_NUMLOCKCLEAR,
    Key_KP_DIVIDE,
    Key_KP_MULTIPLY,
    Key_KP_MINUS,
    Key_KP_PLUS,
    Key_KP_ENTER,
    Key_KP_1,
    Key_KP_2,
    Key_KP_3,
    Key_KP_4,
    Key_KP_5,
    Key_KP_6,
    Key_KP_7,
    Key_KP_8,
    Key_KP_9,
    Key_KP_0,
    Key_KP_PERIOD,

    Key_LCTRL,
    Key_LSHIFT,
    Key_LALT,
    Key_LGUI,
    Key_RCTRL,
    Key_RSHIFT,
    Key_RALT,
    Key_RGUI,

    NUM_KEYBOARD_KEYS
};

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
