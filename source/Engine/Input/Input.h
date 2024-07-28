#ifndef INPUT_H
#define INPUT_H

enum Keyboard {
    Key_UNKNOWN = -1,

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

#define NUM_INPUT_PLAYERS 8
#define NUM_TOUCH_STATES 8
#define DEFAULT_DIGITAL_AXIS_THRESHOLD 0.5

enum InputDevice {
    InputDevice_Keyboard,
    InputDevice_Controller,
    InputDevice_MAX
};

struct ControllerBind {
    int Button;
    int Axis;
    double AxisDeadzone;
    double AxisDigitalThreshold;
    bool IsAxisNegative;

    ControllerBind() {
        Clear();
    }

    void Clear() {
        Button = -1;
        Axis = -1;
        AxisDeadzone = 0.0;
        AxisDigitalThreshold = DEFAULT_DIGITAL_AXIS_THRESHOLD;
        IsAxisNegative = false;
    }
};

struct PlayerInputStatus {
    vector<bool> Held;
    vector<bool> Pressed;
    vector<bool> Released;

    bool AnyHeld;
    bool AnyPressed;
    bool AnyReleased;

    void SetNumActions(size_t num) {
        size_t oldNum = Held.size();

        Pressed.resize(num);
        Held.resize(num);
        Released.resize(num);

        for (size_t i = oldNum; i < num; i++) {
            Pressed[i] = Held[i] = Released[i] = false;
        }
    }

    void Reset() {
        AnyHeld = AnyPressed = AnyReleased = false;

        for (size_t i = 0; i < Held.size(); i++) {
            Pressed[i] = Held[i] = Released[i] = false;
        }
    }
};

struct PlayerInputConfig {
    int KeyboardBind;

    struct ControllerBind ControllerBind;

    PlayerInputConfig() {
        Clear();
    }

    void Clear() {
        KeyboardBind = -1;
        ControllerBind.Clear();
    }
};

#endif /* INPUT_H */
