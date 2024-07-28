#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Includes/BijectiveMap.h>
#include <Engine/Input/Input.h>
#include <Engine/Input/Controller.h>
#include <Engine/Input/ControllerRumble.h>
#include <Engine/Input/InputPlayer.h>
#include <Engine/Input/InputAction.h>
#include <Engine/Application.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/TextFormats/XML/XMLNode.h>

class InputManager {
public:
    static float               MouseX;
    static float               MouseY;
    static int                 MouseDown;
    static int                 MousePressed;
    static int                 MouseReleased;

    static Uint8               KeyboardState[0x120];
    static Uint8               KeyboardStateLast[0x120];
    static SDL_Scancode        KeyToSDLScancode[NUM_KEYBOARD_KEYS];

    static int                 NumControllers;
    static vector<Controller*> Controllers;

    static SDL_TouchID         TouchDevice;
    static void*               TouchStates;

    static vector<InputPlayer> Players;
    static vector<InputAction> Actions;
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
SDL_Scancode        InputManager::KeyToSDLScancode[NUM_KEYBOARD_KEYS];

int                 InputManager::NumControllers;
vector<Controller*> InputManager::Controllers;

SDL_TouchID         InputManager::TouchDevice;
void*               InputManager::TouchStates;

vector<InputPlayer> InputManager::Players;
vector<InputAction> InputManager::Actions;

struct TouchState {
    float X;
    float Y;
    bool  Down;
    bool  Pressed;
    bool  Released;
};

PUBLIC STATIC void  InputManager::Init() {
    memset(KeyboardState, 0, NUM_KEYBOARD_KEYS);
    memset(KeyboardStateLast, 0, NUM_KEYBOARD_KEYS);

    InputManager::InitStringLookup();
    InputManager::InitControllers();

    InputManager::TouchStates = Memory::TrackedCalloc("InputManager::TouchStates", NUM_TOUCH_STATES, sizeof(TouchState));
    for (int t = 0; t < NUM_TOUCH_STATES; t++) {
        TouchState* current = &((TouchState*)TouchStates)[t];
        current->X = 0.0f;
        current->Y = 0.0f;
        current->Down = false;
        current->Pressed = false;
        current->Released = false;
    }
}

namespace NameMap {
    BijectiveMap<const char*, Uint16>* Keys;
    BijectiveMap<const char*, Uint8>*  Buttons;
    BijectiveMap<const char*, Uint8>*  Axes;
}

PRIVATE STATIC void InputManager::InitStringLookup() {
    NameMap::Keys = new BijectiveMap<const char*, Uint16>();
    NameMap::Buttons = new BijectiveMap<const char*, Uint8>();
    NameMap::Axes = new BijectiveMap<const char*, Uint8>();

    #define DEF_KEY(key) { \
        InputManager::KeyToSDLScancode[Key_##key] = SDL_SCANCODE_##key; \
        NameMap::Keys->Put(#key, Key_##key); \
    }
    DEF_KEY(A);
    DEF_KEY(B);
    DEF_KEY(C);
    DEF_KEY(D);
    DEF_KEY(E);
    DEF_KEY(F);
    DEF_KEY(G);
    DEF_KEY(H);
    DEF_KEY(I);
    DEF_KEY(J);
    DEF_KEY(K);
    DEF_KEY(L);
    DEF_KEY(M);
    DEF_KEY(N);
    DEF_KEY(O);
    DEF_KEY(P);
    DEF_KEY(Q);
    DEF_KEY(R);
    DEF_KEY(S);
    DEF_KEY(T);
    DEF_KEY(U);
    DEF_KEY(V);
    DEF_KEY(W);
    DEF_KEY(X);
    DEF_KEY(Y);
    DEF_KEY(Z);

    DEF_KEY(1);
    DEF_KEY(2);
    DEF_KEY(3);
    DEF_KEY(4);
    DEF_KEY(5);
    DEF_KEY(6);
    DEF_KEY(7);
    DEF_KEY(8);
    DEF_KEY(9);
    DEF_KEY(0);

    DEF_KEY(RETURN);
    DEF_KEY(ESCAPE);
    DEF_KEY(BACKSPACE);
    DEF_KEY(TAB);
    DEF_KEY(SPACE);

    DEF_KEY(MINUS);
    DEF_KEY(EQUALS);
    DEF_KEY(LEFTBRACKET);
    DEF_KEY(RIGHTBRACKET);
    DEF_KEY(BACKSLASH);
    DEF_KEY(SEMICOLON);
    DEF_KEY(APOSTROPHE);
    DEF_KEY(GRAVE);
    DEF_KEY(COMMA);
    DEF_KEY(PERIOD);
    DEF_KEY(SLASH);

    DEF_KEY(CAPSLOCK);

    DEF_KEY(F1);
    DEF_KEY(F2);
    DEF_KEY(F3);
    DEF_KEY(F4);
    DEF_KEY(F5);
    DEF_KEY(F6);
    DEF_KEY(F7);
    DEF_KEY(F8);
    DEF_KEY(F9);
    DEF_KEY(F10);
    DEF_KEY(F11);
    DEF_KEY(F12);

    DEF_KEY(PRINTSCREEN);
    DEF_KEY(SCROLLLOCK);
    DEF_KEY(PAUSE);
    DEF_KEY(INSERT);
    DEF_KEY(HOME);
    DEF_KEY(PAGEUP);
    DEF_KEY(DELETE);
    DEF_KEY(END);
    DEF_KEY(PAGEDOWN);
    DEF_KEY(RIGHT);
    DEF_KEY(LEFT);
    DEF_KEY(DOWN);
    DEF_KEY(UP);

    DEF_KEY(NUMLOCKCLEAR);
    DEF_KEY(KP_DIVIDE);
    DEF_KEY(KP_MULTIPLY);
    DEF_KEY(KP_MINUS);
    DEF_KEY(KP_PLUS);
    DEF_KEY(KP_ENTER);
    DEF_KEY(KP_1);
    DEF_KEY(KP_2);
    DEF_KEY(KP_3);
    DEF_KEY(KP_4);
    DEF_KEY(KP_5);
    DEF_KEY(KP_6);
    DEF_KEY(KP_7);
    DEF_KEY(KP_8);
    DEF_KEY(KP_9);
    DEF_KEY(KP_0);
    DEF_KEY(KP_PERIOD);

    DEF_KEY(LCTRL);
    DEF_KEY(LSHIFT);
    DEF_KEY(LALT);
    DEF_KEY(LGUI);
    DEF_KEY(RCTRL);
    DEF_KEY(RSHIFT);
    DEF_KEY(RALT);
    DEF_KEY(RGUI);

    NameMap::Keys->Put("UNKNOWN", Key_UNKNOWN);

    #undef DEF_KEY

    #define DEF_BUTTON(x, y) NameMap::Buttons->Put(#x, (int)ControllerButton::y)
    DEF_BUTTON(A, A);
    DEF_BUTTON(B, B);
    DEF_BUTTON(X, X);
    DEF_BUTTON(Y, Y);
    DEF_BUTTON(BACK, Back);
    DEF_BUTTON(GUIDE, Guide);
    DEF_BUTTON(START, Start);
    DEF_BUTTON(LEFTSTICK, LeftStick);
    DEF_BUTTON(RIGHTSTICK, RightStick);
    DEF_BUTTON(LEFTSHOULDER, LeftShoulder);
    DEF_BUTTON(RIGHTSHOULDER, RightShoulder);
    DEF_BUTTON(DPAD_UP, DPadUp);
    DEF_BUTTON(DPAD_DOWN, DPadDown);
    DEF_BUTTON(DPAD_LEFT, DPadLeft);
    DEF_BUTTON(DPAD_RIGHT, DPadRight);
    DEF_BUTTON(SHARE, Share);
    DEF_BUTTON(MICROPHONE, Microphone);
    DEF_BUTTON(TOUCHPAD, Touchpad);
    DEF_BUTTON(PADDLE1, Paddle1);
    DEF_BUTTON(PADDLE2, Paddle2);
    DEF_BUTTON(PADDLE3, Paddle3);
    DEF_BUTTON(PADDLE4, Paddle4);
    DEF_BUTTON(MISC1, Misc1);
    #undef DEF_BUTTON

    #define DEF_AXIS(x, y) NameMap::Axes->Put(#x, (int)ControllerAxis::y)
    DEF_AXIS(LEFTX, LeftX);
    DEF_AXIS(LEFTY, LeftY);
    DEF_AXIS(RIGHTX, RightX);
    DEF_AXIS(RIGHTY, RightY);
    DEF_AXIS(TRIGGERLEFT, TriggerLeft);
    DEF_AXIS(TRIGGERRIGHT, TriggerRight);
    #undef DEF_AXIS
}

PUBLIC STATIC char* InputManager::GetKeyName(int key) {
    if (!NameMap::Keys->Exists(key))
        return nullptr;
    return (char*)NameMap::Keys->Get(key);
}
PUBLIC STATIC char* InputManager::GetButtonName(int button) {
    if (!NameMap::Buttons->Exists(button))
        return nullptr;
    return (char*)NameMap::Buttons->Get(button);
}
PUBLIC STATIC char* InputManager::GetAxisName(int axis) {
    if (!NameMap::Axes->Exists(axis))
        return nullptr;
    return (char*)NameMap::Axes->Get(axis);
}

#define FIND_IN_BIJECTIVE(which, search) { \
    int found = -1; \
    which->WithAllKeys([search, &found](const char* k, Uint16 v) -> bool { \
        if (!strcmp(search, k)) { \
            found = v; \
            return true; \
        } \
        return false; \
    }); \
    return found; \
}

PUBLIC STATIC int InputManager::ParseKeyName(const char* key) {
    FIND_IN_BIJECTIVE(NameMap::Keys, key);
}
PUBLIC STATIC int InputManager::ParseButtonName(const char* button) {
    FIND_IN_BIJECTIVE(NameMap::Buttons, button);
}
PUBLIC STATIC int InputManager::ParseAxisName(const char* axis) {
    FIND_IN_BIJECTIVE(NameMap::Axes, axis);
}

#undef FIND_IN_BIJECTIVE

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

    for (size_t i = 0; i < InputManager::Players.size(); i++)
        InputManager::Players[i].Update();
}

PUBLIC STATIC bool  InputManager::IsKeyDown(int key) {
    int scancode = (int)KeyToSDLScancode[key];
    return KeyboardState[scancode];
}
PUBLIC STATIC bool  InputManager::IsKeyPressed(int key) {
    int scancode = (int)KeyToSDLScancode[key];
    return KeyboardState[scancode] && !KeyboardStateLast[scancode];
}
PUBLIC STATIC bool  InputManager::IsKeyReleased(int key) {
    int scancode = (int)KeyToSDLScancode[key];
    return !KeyboardState[scancode] && KeyboardStateLast[scancode];
}

PUBLIC STATIC Controller* InputManager::GetController(int index) {
    if (index >= 0 && index < InputManager::NumControllers) {
        return InputManager::Controllers[index];
    }
    return nullptr;
}

PUBLIC STATIC bool  InputManager::ControllerIsConnected(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->Connected;
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerIsXbox(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->IsXbox();
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerIsPlayStation(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->IsPlayStation();
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerIsJoyCon(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->IsJoyCon();
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerHasShareButton(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->HasShareButton();
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerHasMicrophoneButton(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->HasMicrophoneButton();
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerHasPaddles(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->HasPaddles();
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerIsButtonHeld(int index, int button) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->IsButtonHeld(button);
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerIsButtonPressed(int index, int button) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->IsButtonPressed(button);
    return false;
}
PUBLIC STATIC float InputManager::ControllerGetAxis(int index, int axis) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->GetAxis(axis);
    return 0.0f;
}
PUBLIC STATIC int   InputManager::ControllerGetType(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return (int)controller->Type;
    return (int)ControllerType::Unknown;
}
PUBLIC STATIC char* InputManager::ControllerGetName(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->GetName();
    return nullptr;
}
PUBLIC STATIC void  InputManager::ControllerSetPlayerIndex(int index, int player_index) {
    Controller* controller = GetController(index);
    if (controller)
        controller->SetPlayerIndex(player_index);
}
PUBLIC STATIC bool  InputManager::ControllerHasRumble(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->Rumble != nullptr;
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerIsRumbleActive(int index) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        return controller->Rumble->Active;
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerRumble(int index, float large_frequency, float small_frequency, int duration) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        return controller->Rumble->Enable(large_frequency, small_frequency, (Uint32)duration);
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerRumble(int index, float strength, int duration) {
    return ControllerRumble(index, strength, strength, duration);
}
PUBLIC STATIC void  InputManager::ControllerStopRumble(int index) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        controller->Rumble->Stop();
}
PUBLIC STATIC void  InputManager::ControllerStopRumble() {
    for (int i = 0; i < InputManager::NumControllers; i++) {
        Controller* controller = InputManager::Controllers[i];
        if (!controller || !controller->Rumble)
            continue;

        if (controller->Rumble->Active)
            controller->Rumble->Stop();
    }
}
PUBLIC STATIC bool  InputManager::ControllerIsRumblePaused(int index) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        return controller->Rumble->Paused;
    return false;
}
PUBLIC STATIC void  InputManager::ControllerSetRumblePaused(int index, bool paused) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        controller->Rumble->SetPaused(paused);
}
PUBLIC STATIC bool  InputManager::ControllerSetLargeMotorFrequency(int index, float frequency) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        return controller->Rumble->SetLargeMotorFrequency(frequency);
    return false;
}
PUBLIC STATIC bool  InputManager::ControllerSetSmallMotorFrequency(int index, float frequency) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        return controller->Rumble->SetSmallMotorFrequency(frequency);
    return false;
}

PUBLIC STATIC float InputManager::TouchGetX(int touch_index) {
    if (touch_index < 0 || touch_index >= NUM_TOUCH_STATES)
        return 0.0f;
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].X;
}
PUBLIC STATIC float InputManager::TouchGetY(int touch_index) {
    if (touch_index < 0 || touch_index >= NUM_TOUCH_STATES)
        return 0.0f;
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Y;
}
PUBLIC STATIC bool  InputManager::TouchIsDown(int touch_index) {
    if (touch_index < 0 || touch_index >= NUM_TOUCH_STATES)
        return false;
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Down;
}
PUBLIC STATIC bool  InputManager::TouchIsPressed(int touch_index) {
    if (touch_index < 0 || touch_index >= NUM_TOUCH_STATES)
        return false;
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Pressed;
}
PUBLIC STATIC bool  InputManager::TouchIsReleased(int touch_index) {
    if (touch_index < 0 || touch_index >= NUM_TOUCH_STATES)
        return false;
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Released;
}

PUBLIC STATIC int   InputManager::AddPlayer() {
    int id = (int)InputManager::Players.size();

    InputPlayer player(id);

    player.SetNumActions(InputManager::Actions.size());

    InputManager::Players.push_back(player);

    return id;
}
PUBLIC STATIC void  InputManager::SetPlayerControllerIndex(unsigned playerID, int index) {
    if (playerID >= InputManager::Players.size())
        return;

    if (index < 0)
        index = -1;

    InputManager::Players[playerID].ControllerIndex = index;
}
PUBLIC STATIC int   InputManager::GetPlayerControllerIndex(unsigned playerID) {
    if (playerID >= InputManager::Players.size())
        return -1;

    return InputManager::Players[playerID].ControllerIndex;
}
PUBLIC STATIC bool  InputManager::IsActionHeld(unsigned playerID, unsigned actionID) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsInputHeld(actionID);
}
PUBLIC STATIC bool  InputManager::IsActionPressed(unsigned playerID, unsigned actionID) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsInputPressed(actionID);
}
PUBLIC STATIC bool  InputManager::IsActionReleased(unsigned playerID, unsigned actionID) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsInputReleased(actionID);
}
PUBLIC STATIC bool  InputManager::IsAnyActionHeld(unsigned playerID) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsAnyInputHeld();
}
PUBLIC STATIC bool  InputManager::IsAnyActionPressed(unsigned playerID) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsAnyInputPressed();
}
PUBLIC STATIC bool  InputManager::IsAnyActionReleased(unsigned playerID) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsAnyInputReleased();
}
PUBLIC STATIC bool  InputManager::IsActionHeld(unsigned playerID, unsigned actionID, unsigned device) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsInputHeld(actionID, device);
}
PUBLIC STATIC bool  InputManager::IsActionPressed(unsigned playerID, unsigned actionID, unsigned device) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsInputPressed(actionID, device);
}
PUBLIC STATIC bool  InputManager::IsActionReleased(unsigned playerID, unsigned actionID, unsigned device) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsInputReleased(actionID, device);
}
PUBLIC STATIC bool  InputManager::IsAnyActionHeld(unsigned playerID, unsigned device) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsAnyInputHeld(device);
}
PUBLIC STATIC bool  InputManager::IsAnyActionPressed(unsigned playerID, unsigned device) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsAnyInputPressed(device);
}
PUBLIC STATIC bool  InputManager::IsAnyActionReleased(unsigned playerID, unsigned device) {
    if (playerID >= InputManager::Players.size())
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsAnyInputReleased(device);
}
PUBLIC STATIC bool  InputManager::IsPlayerUsingDevice(unsigned playerID, unsigned device) {
    if (device >= InputDevice_Controller)
        return false;

    InputPlayer& player = InputManager::Players[playerID];

    return player.IsUsingDevice[device];
}
PUBLIC STATIC float InputManager::GetAnalogActionInput(unsigned playerID, unsigned actionID) {
    if (playerID >= InputManager::Players.size())
        return 0.0f;

    InputPlayer& player = InputManager::Players[playerID];

    return player.GetAnalogActionInput(actionID);
}
PUBLIC STATIC void  InputManager::ClearPlayers() {
    InputManager::Players.clear();
}
PUBLIC STATIC int   InputManager::RegisterAction(const char* name) {
    int id = InputManager::GetActionID(name);
    if (id != -1)
        return id;

    id = (int)InputManager::Actions.size();

    InputAction input(name, id);

    InputManager::Actions.push_back(input);

    for (size_t i = 0; i < InputManager::Players.size(); i++)
        InputManager::Players[i].SetNumActions(InputManager::Actions.size());

    return id;
}
PUBLIC STATIC int   InputManager::GetActionID(const char* name) {
    if (name != nullptr && name[0] != '\0') {
        for (size_t i = 0; i < InputManager::Actions.size(); i++) {
            if (strcmp(InputManager::Actions[i].Name.c_str(), name) == 0)
                return (int)i;
        }
    }

    return -1;
}
PUBLIC STATIC void  InputManager::ClearInputs() {
    InputManager::Actions.clear();

    for (size_t i = 0; i < InputManager::Players.size(); i++)
        InputManager::Players[i].SetNumActions(0);
}

PUBLIC STATIC void  InputManager::InitPlayerControls() {
    if (!Application::GameConfig)
        return;

    XMLNode* controlsNode = XMLParser::SearchNode(Application::GameConfig->children[0], "controls");
    if (!controlsNode)
        return;

    // Count players
    int numPlayers = 1;
    for (size_t i = 0; i < controlsNode->children.size(); i++) {
        XMLNode* child = controlsNode->children[i];

        int id = 0;

        if (XMLParser::MatchToken(child->name, "player")
        && child->attributes.Exists("id")
        && StringUtils::ToNumber(&id, child->attributes.Get("id").ToString())
        && id > numPlayers && id < NUM_INPUT_PLAYERS)
            numPlayers = id;
    }

    // Add players
    for (int i = 0; i < numPlayers; i++)
        InputManager::AddPlayer();

    // Parse inputs
    for (size_t i = 0; i < controlsNode->children.size(); i++) {
        XMLNode* child = controlsNode->children[i];
        if (XMLParser::MatchToken(child->name, "input")) {
            if (child->children[0]->name.Length > 0) {
                Token& name = child->children[0]->name;
                InputManager::RegisterAction(name.ToString().c_str());
            }
        }
    }

    // Parse players
    for (size_t i = 0; i < controlsNode->children.size(); i++) {
        XMLNode* child = controlsNode->children[i];
        if (XMLParser::MatchToken(child->name, "player")) {
            if (!child->attributes.Exists("id"))
                continue;

            int id = 0;
            if (!StringUtils::ToNumber(&id, child->attributes.Get("id").ToString()))
                continue;

            if (id < 1 || id > numPlayers)
                continue;

            InputPlayer& player = InputManager::Players[id - 1];

            player.ControllerIndex = id - 1;

            ParsePlayerControls(player, child);

            player.ResetBinds();
        }
    }
}

PRIVATE STATIC void InputManager::ParsePlayerControls(InputPlayer& player, XMLNode* node) {
    // Parse keyboard controls
    XMLNode* controlsNode = XMLParser::SearchNode(node, "keyboard");
    if (controlsNode) {
        for (size_t i = 0; i < controlsNode->children.size(); i++) {
            XMLNode* child = controlsNode->children[i];
            int actionID = InputManager::GetActionID(child->name.ToString().c_str());
            if (actionID == -1)
                continue;

            if (!child->children[0]->name.Length)
                continue;

            int keyID = InputManager::ParseKeyName(child->children[0]->name.ToString().c_str());
            if (keyID != -1)
                player.SetDefaultKeyboardBind(actionID, keyID);
        }
    }

    // Parse controller controls
    controlsNode = XMLParser::SearchNode(node, "controller");
    if (controlsNode) {
        for (size_t i = 0; i < controlsNode->children.size(); i++) {
            XMLNode* child = controlsNode->children[i];
            int actionID = InputManager::GetActionID(child->name.ToString().c_str());
            if (actionID == -1)
                continue;

            ControllerBind bind;

            if (child->children.size() == 1 && !child->children[0]->children.size()) {
                bind.Button = InputManager::ParseButtonName(child->children[0]->name.ToString().c_str());
            }
            else {
                bind = ParseControllerBind(child);
            }

            player.SetDefaultControllerBind(actionID, bind);
        }
    }
}

PRIVATE STATIC ControllerBind InputManager::ParseControllerBind(XMLNode* node) {
    ControllerBind bind;

    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (XMLParser::MatchToken(child->name, "button")) {
            bind.Button = InputManager::ParseButtonName(child->children[0]->name.ToString().c_str());
        }
        else if (XMLParser::MatchToken(child->name, "axis")) {
            const char* axisName = child->children[0]->name.ToString().c_str();

            int axisID;
            bool isNegative = false;

            if (axisName[0] == '-' || axisName[0] == '+') {
                isNegative = axisName[0] == '-';
                axisID = InputManager::ParseAxisName(&axisName[1]);
            }
            else
                axisID = InputManager::ParseAxisName(axisName);

            if (axisID != -1) {
                bind.Axis = axisID;
                bind.IsAxisNegative = isNegative;
            }

            if (child->attributes.Exists("deadzone")) {
                double deadzone;
                if (StringUtils::ToDecimal(&deadzone, child->attributes.Get("deadzone").ToString())) {
                    bind.AxisDeadzone = deadzone;
                }
            }

            if (child->attributes.Exists("digital_threshold")) {
                double threshold;
                if (StringUtils::ToDecimal(&threshold, child->attributes.Get("digital_threshold").ToString())) {
                    bind.AxisDigitalThreshold = threshold;
                }
            }
        }
    }

    return bind;
}

PUBLIC STATIC void  InputManager::Dispose() {
    InputManager::ControllerStopRumble();

    // Close controllers
    for (int i = 0; i < InputManager::NumControllers; i++) {
        Log::Print(Log::LOG_VERBOSE, "Closing controller %d", i);
        delete InputManager::Controllers[i];
    }

    // Dispose of touch states
    Memory::Free(InputManager::TouchStates);

    InputManager::ClearPlayers();
    InputManager::ClearInputs();
}
