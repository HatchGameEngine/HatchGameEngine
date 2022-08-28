#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Input/Controller.h>
#include <Engine/Application.h>

class InputManager {
public:
    static float               MouseX;
    static float               MouseY;
    static int                 MouseDown;
    static int                 MousePressed;
    static int                 MouseReleased;

    static Uint8               KeyboardState[0x120];
    static Uint8               KeyboardStateLast[0x120];

    static int                 NumControllers;
    static vector<Controller*> Controllers;

    static SDL_TouchID         TouchDevice;
    static void*               TouchStates;
};
#endif

#include <Engine/InputManager.h>

float               InputManager::MouseX = 0;
float               InputManager::MouseY = 0;
int                 InputManager::MouseDown = 0;
int                 InputManager::MousePressed = 0;
int                 InputManager::MouseReleased = 0;

Uint8               InputManager::KeyboardState[0x120];
Uint8               InputManager::KeyboardStateLast[0x120];

int                 InputManager::NumControllers;
vector<Controller*> InputManager::Controllers;

SDL_TouchID         InputManager::TouchDevice;
void*               InputManager::TouchStates;

struct TouchState {
    float X;
    float Y;
    bool Down;
    bool Pressed;
    bool Released;
};

PUBLIC STATIC void  InputManager::Init() {
    memset(KeyboardState, 0, 0x120);
    memset(KeyboardStateLast, 0, 0x120);

    InputManager::InitControllers();

    InputManager::TouchStates = Memory::TrackedCalloc("InputManager::TouchStates", 8, sizeof(TouchState));
    for (int t = 0; t < 8; t++) {
        TouchState* current = &((TouchState*)TouchStates)[t];
        current->X = 0.0f;
        current->Y = 0.0f;
        current->Down = false;
        current->Pressed = false;
        current->Released = false;
    }
}

PUBLIC STATIC Controller* InputManager::OpenController(int index) {
    Controller* controller = new Controller(index);
    if (controller->Device == nullptr) {
        Log::Print(Log::LOG_ERROR, "Opening controller %d failed: %s", index, SDL_GetError());
        delete controller;
    }

    Log::Print(Log::LOG_VERBOSE, "Controller %d opened", index);

    return controller;
}

PUBLIC STATIC void  InputManager::InitControllers() {
    int numControllers = 0;
    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; i++) {
        if (SDL_IsGameController(i))
            numControllers++;
    }

    Log::Print(Log::LOG_VERBOSE, "Opening controllers... (%d count)", numControllers);

    InputManager::Controllers.resize(0);
    InputManager::NumControllers = 0;

    for (int i = 0; i < numJoysticks; i++) {
        if (!SDL_IsGameController(i))
            continue;

        Controller* controller = InputManager::OpenController(i);
        if (controller) {
            InputManager::Controllers.push_back(controller);
            InputManager::NumControllers++;
        }
    }
}

PRIVATE STATIC int InputManager::FindController(int joystickID) {
    for (int i = 0; i < InputManager::NumControllers; i++) {
        Controller* controller = InputManager::Controllers[i];
        if (controller->Connected && controller->JoystickID == joystickID)
            return i;
    }

    return -1;
}

PUBLIC STATIC bool  InputManager::AddController(int index) {
    Controller* controller;
    for (int i = 0; i < InputManager::NumControllers; i++) {
        controller = InputManager::Controllers[i];
        if (!controller->Connected)
            return controller->Open(index);
    }

    controller = InputManager::OpenController(index);
    if (!controller)
        return false;

    InputManager::Controllers.push_back(controller);
    InputManager::NumControllers = InputManager::Controllers.size();

    return true;
}

PUBLIC STATIC void  InputManager::RemoveController(int joystickID) {
    int controller_id = InputManager::FindController(joystickID);
    if (controller_id == -1)
        return;

    InputManager::Controllers[controller_id]->Close();
}

PUBLIC STATIC void  InputManager::Poll() {
    if (Application::Platform == Platforms::iOS ||
        Application::Platform == Platforms::Android ||
        Application::Platform == Platforms::Switch) {
        int w, h;
        TouchState* states = (TouchState*)TouchStates;
        SDL_GetWindowSize(Application::Window, &w, &h);

        bool OneDown = false;
        for (int d = 0; d < SDL_GetNumTouchDevices(); d++) {
            TouchDevice = SDL_GetTouchDevice(d);
            if (TouchDevice) {
                OneDown = false;
                for (int t = 0; t < 8 && SDL_GetNumTouchFingers(TouchDevice); t++) {
                    TouchState* current = &states[t];

                    bool previouslyDown = current->Down;

                    current->Down = false;
                    if (t < SDL_GetNumTouchFingers(TouchDevice)) {
                        SDL_Finger* finger = SDL_GetTouchFinger(TouchDevice, t);
                        float tx = finger->x * w;
                        float ty = finger->y * h;

                        current->X = tx;
                        current->Y = ty;
                        current->Down = true;

                        OneDown = true;
                    }

                    current->Pressed = !previouslyDown && current->Down;
                    current->Released = previouslyDown && !current->Down;
                }

                if (OneDown)
                    break;
            }
        }

        // NOTE: If no touches were down, set all their
        //   states to !Down and Released appropriately.
        if (!OneDown) {
            for (int t = 0; t < 8; t++) {
                TouchState* current = &states[t];

                bool previouslyDown = current->Down;

                current->Down = false;
                current->Pressed = !previouslyDown && current->Down;
                current->Released = previouslyDown && !current->Down;
            }
        }
    }

    const Uint8* state = SDL_GetKeyboardState(NULL);
    memcpy(KeyboardStateLast, KeyboardState, 0x11C + 1);
    memcpy(KeyboardState, state, 0x11C + 1);

    int mx, my, buttons;
    buttons = SDL_GetMouseState(&mx, &my);
    MouseX = mx;
    MouseY = my;

    int lastDown = MouseDown;
    MouseDown = 0;
    MousePressed = 0;
    MouseReleased = 0;
    for (int i = 0; i < 8; i++) {
        int lD, mD, mP, mR;
        lD = (lastDown >> i) & 1;
        mD = (buttons >> i) & 1;
        mP = !lD && mD;
        mR = lD && !mD;

        MouseDown |= mD << i;
        MousePressed |= mP << i;
        MouseReleased |= mR << i;
    }

    for (int i = 0; i < InputManager::NumControllers; i++) {
        Controller* controller = InputManager::Controllers[i];
        if (controller->Connected)
            controller->Update();
    }
}

#define GET_CONTROLLER(ret) \
    if (index < 0 || index >= InputManager::NumControllers) \
        return ret; \
    Controller* controller = InputManager::Controllers[index]; \
    if (!controller) return ret

PUBLIC STATIC bool  InputManager::ControllerIsConnected(int index) {
    GET_CONTROLLER(false);
    return controller->Connected;
}
PUBLIC STATIC bool  InputManager::ControllerGetButton(int index, int button) {
    GET_CONTROLLER(false);
    return controller->GetButton(button);
}
// For backwards compatibility. Use ControllerGetButton instead.
PUBLIC STATIC int   InputManager::ControllerGetHat(int index, int hat) {
    if (hat != 0) return 0;
    GET_CONTROLLER(0);

    int flags = 0;
    if (controller->GetButton((int)ControllerButton::DPadUp))
        flags |= SDL_HAT_UP;
    if (controller->GetButton((int)ControllerButton::DPadDown))
        flags |= SDL_HAT_DOWN;
    if (controller->GetButton((int)ControllerButton::DPadLeft))
        flags |= SDL_HAT_LEFT;
    if (controller->GetButton((int)ControllerButton::DPadRight))
        flags |= SDL_HAT_RIGHT;

    return flags;
}
PUBLIC STATIC float InputManager::ControllerGetAxis(int index, int axis) {
    GET_CONTROLLER(0.0f);
    return controller->GetAxis(axis);
}
PUBLIC STATIC int   InputManager::ControllerGetType(int index) {
    GET_CONTROLLER((int)ControllerType::Unknown);
    return (int)controller->Type;
}
PUBLIC STATIC char* InputManager::ControllerGetName(int index) {
    GET_CONTROLLER(nullptr);
    return controller->GetName();
}
PUBLIC STATIC void  InputManager::ControllerSetPlayerIndex(int index, int player_index) {
    GET_CONTROLLER();
    controller->SetPlayerIndex(player_index);
}
PUBLIC STATIC bool  InputManager::ControllerHasRumble(int index) {
    GET_CONTROLLER(false);
    return controller->Rumble != nullptr;
}
PUBLIC STATIC bool  InputManager::ControllerIsRumbleActive(int index) {
    GET_CONTROLLER(false);
    if (!controller->Rumble) return false;
    return controller->Rumble->Active;
}
PUBLIC STATIC bool  InputManager::ControllerRumble(int index, float large_frequency, float small_frequency, int duration) {
    GET_CONTROLLER(false);
    if (!controller->Rumble) return false;
    return controller->Rumble->Enable(large_frequency, small_frequency, (Uint32)duration);
}
PUBLIC STATIC bool  InputManager::ControllerRumble(int index, float strength, int duration) {
    return InputManager::ControllerRumble(index, strength, strength, duration);
}
PUBLIC STATIC void  InputManager::ControllerStopRumble(int index) {
    GET_CONTROLLER();
    if (!controller->Rumble) return;
    controller->Rumble->Stop();
}
PUBLIC STATIC bool  InputManager::ControllerGetRumblePaused(int index) {
    GET_CONTROLLER(false);
    if (!controller->Rumble) return false;
    return controller->Rumble->Paused;
}
PUBLIC STATIC void  InputManager::ControllerSetRumblePaused(int index, bool paused) {
    GET_CONTROLLER();
    if (!controller->Rumble) return;
    controller->Rumble->SetPaused(paused);
}
PUBLIC STATIC bool  InputManager::ControllerSetLargeMotorFrequency(int index, float frequency) {
    GET_CONTROLLER(false);
    if (!controller->Rumble) return false;
    return controller->Rumble->SetLargeMotorFrequency(frequency);
}
PUBLIC STATIC bool  InputManager::ControllerSetSmallMotorFrequency(int index, float frequency) {
    GET_CONTROLLER(false);
    if (!controller->Rumble) return false;
    return controller->Rumble->SetSmallMotorFrequency(frequency);
}

#undef GET_CONTROLLER

PUBLIC STATIC float InputManager::TouchGetX(int touch_index) {
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].X;
}
PUBLIC STATIC float InputManager::TouchGetY(int touch_index) {
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Y;
}
PUBLIC STATIC bool  InputManager::TouchIsDown(int touch_index) {
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Down;
}
PUBLIC STATIC bool  InputManager::TouchIsPressed(int touch_index) {
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Pressed;
}
PUBLIC STATIC bool  InputManager::TouchIsReleased(int touch_index) {
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Released;
}

PUBLIC STATIC void  InputManager::Dispose() {
    // Close controllers
    for (int i = 0; i < InputManager::NumControllers; i++) {
        Log::Print(Log::LOG_VERBOSE, "Closing controller %d", i);
        delete InputManager::Controllers[i];
    }

    // Dispose of touch states
    Memory::Free(InputManager::TouchStates);
}
