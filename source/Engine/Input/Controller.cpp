#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Input/Input.h>
// #include <Engine/Application.h>

class Controller {
public:
    ControllerType      Type;
    bool                Connected;

    ControllerRumble*   Rumble;

    SDL_GameController* Device;
    SDL_Joystick*       JoystickDevice;
    SDL_JoystickID      JoystickID;
};
#endif

#include <Engine/InputManager.h>
#include <Engine/Input/Controller.h>

PUBLIC Controller::Controller(int index) {
    Reset();
    Open(index);
}

PUBLIC Controller::~Controller() {
    Close();
}

#define CONST_BUTTON(x, y) [(int)ControllerButton::x] = SDL_CONTROLLER_BUTTON_##y

static SDL_GameControllerButton ButtonEnums[] = {
    CONST_BUTTON(A, A),
    CONST_BUTTON(B, B),
    CONST_BUTTON(X, X),
    CONST_BUTTON(Y, Y),
    CONST_BUTTON(Back, BACK),
    CONST_BUTTON(Guide, GUIDE),
    CONST_BUTTON(Start, START),
    CONST_BUTTON(LeftStick, LEFTSTICK),
    CONST_BUTTON(RightStick, RIGHTSTICK),
    CONST_BUTTON(LeftShoulder, LEFTSHOULDER),
    CONST_BUTTON(RightShoulder, RIGHTSHOULDER),
    CONST_BUTTON(DPadUp, DPAD_UP),
    CONST_BUTTON(DPadDown, DPAD_DOWN),
    CONST_BUTTON(DPadLeft, DPAD_LEFT),
    CONST_BUTTON(DPadRight, DPAD_RIGHT),
    CONST_BUTTON(Share, MISC1),
    CONST_BUTTON(Microphone, MISC1),
    CONST_BUTTON(Touchpad, TOUCHPAD),
    CONST_BUTTON(Paddle1, PADDLE1),
    CONST_BUTTON(Paddle2, PADDLE2),
    CONST_BUTTON(Paddle3, PADDLE3),
    CONST_BUTTON(Paddle4, PADDLE4),
    CONST_BUTTON(Misc1, MISC1)
};

#undef CONST_BUTTON
#define CONST_AXIS(x, y) [(int)ControllerAxis::x] = SDL_CONTROLLER_AXIS_##y

static SDL_GameControllerAxis AxisEnums[] = {
    CONST_AXIS(LeftX, LEFTX),
    CONST_AXIS(LeftY, LEFTY),
    CONST_AXIS(RightX, RIGHTX),
    CONST_AXIS(RightY, RIGHTY),
    CONST_AXIS(TriggerLeft, TRIGGERLEFT),
    CONST_AXIS(TriggerRight, TRIGGERRIGHT)
};

#undef CONST_AXIS

PRIVATE STATIC ControllerType Controller::DetermineType(void* gamecontroller) {
    switch (SDL_GameControllerGetType((SDL_GameController*)gamecontroller)) {
        case SDL_CONTROLLER_TYPE_XBOX360:
            return ControllerType::Xbox360;
        case SDL_CONTROLLER_TYPE_XBOXONE:
            return ControllerType::XboxOne;
        case SDL_CONTROLLER_TYPE_PS3:
            return ControllerType::PS3;
        case SDL_CONTROLLER_TYPE_PS4:
            return ControllerType::PS4;
        case SDL_CONTROLLER_TYPE_PS5:
            return ControllerType::PS5;
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
            return ControllerType::SwitchJoyConLeft;
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
            return ControllerType::SwitchJoyConRight;
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            return ControllerType::SwitchJoyConPair;
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
            return ControllerType::SwitchPro;
        case SDL_CONTROLLER_TYPE_GOOGLE_STADIA:
            return ControllerType::Stadia;
        case SDL_CONTROLLER_TYPE_AMAZON_LUNA:
            return ControllerType::AmazonLuna;
        case SDL_CONTROLLER_TYPE_NVIDIA_SHIELD:
            return ControllerType::NvidiaShield;
        case SDL_CONTROLLER_TYPE_UNKNOWN:
            return ControllerType::Unknown;
        default:
            return ControllerType::Xbox360;
    }
}

PUBLIC bool          Controller::Open(int index) {
    Device = SDL_GameControllerOpen(index);
    if (!Device)
        return false;

    Type = Controller::DetermineType(Device);
    JoystickDevice = SDL_GameControllerGetJoystick(Device);
    JoystickID = SDL_JoystickInstanceID(JoystickDevice);
    Connected = true;

    if (SDL_GameControllerHasRumble(Device))
        Rumble = new ControllerRumble(Device);

    return true;
}

PUBLIC void          Controller::Close() {
    if (Rumble) {
        Rumble->Stop();
        delete Rumble;
    }

    SDL_GameControllerClose(Device);
    Reset();
}

PUBLIC void          Controller::Reset() {
    Type = ControllerType::Unknown;
    Connected = false;
    Device = nullptr;
    Rumble = nullptr;
    JoystickDevice = nullptr;
    JoystickID = -1;
}

PUBLIC char*          Controller::GetName() {
    return (char*)SDL_GameControllerName(Device);
}
PUBLIC void           Controller::SetPlayerIndex(int index) {
    SDL_GameControllerSetPlayerIndex(Device, index); // Sets the LEDs in some controllers
}

PUBLIC bool          Controller::GetButton(int button) {
    if (button >= (int)ControllerButton::Max)
        return false;
    return SDL_GameControllerGetButton(Device, ButtonEnums[button]);
}

PUBLIC float          Controller::GetAxis(int axis) {
    if (axis >= (int)ControllerAxis::Max)
        return 0.0f;
    Sint16 value = SDL_GameControllerGetAxis(Device, AxisEnums[axis]);
    return (float)value / 32767;
}

PUBLIC void           Controller::Update() {
    if (Rumble)
        Rumble->Update();
}

PUBLIC bool          Controller::IsXbox() {
    return Type == ControllerType::Xbox360 ||
        Type == ControllerType::XboxOne ||
        Type == ControllerType::XboxSeriesXS ||
        Type == ControllerType::XboxElite;
}
PUBLIC bool          Controller::IsPlaystation() {
    return Type == ControllerType::PS3 ||
        Type == ControllerType::PS4 ||
        Type == ControllerType::PS5;
}
PUBLIC bool          Controller::IsJoyCon() {
    return Type == ControllerType::SwitchJoyConLeft ||
        Type == ControllerType::SwitchJoyConRight;
}
PUBLIC bool          Controller::HasShareButton() {
    return Type == ControllerType::XboxSeriesXS ||
        Type == ControllerType::SwitchPro ||
        Type == ControllerType::SwitchJoyConLeft;
}
PUBLIC bool          Controller::HasMicrophoneButton() {
    return Type == ControllerType::PS5 ||
        Type == ControllerType::AmazonLuna;
}
PUBLIC bool          Controller::HasPaddles() {
    return Type == ControllerType::XboxElite;
}
