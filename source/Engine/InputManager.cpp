#include <Engine/InputManager.h>
#include <Engine/Diagnostics/Log.h>

#include <sstream>

float               InputManager::MouseX = 0;
float               InputManager::MouseY = 0;
int                 InputManager::MouseDown = 0;
int                 InputManager::MousePressed = 0;
int                 InputManager::MouseReleased = 0;

Uint8               InputManager::KeyboardState[0x120];
Uint8               InputManager::KeyboardStateLast[0x120];
Uint16              InputManager::KeymodState;
SDL_Scancode        InputManager::KeyToSDLScancode[NUM_KEYBOARD_KEYS];

int                 InputManager::NumControllers;
vector<Controller*> InputManager::Controllers;

SDL_TouchID         InputManager::TouchDevice;
void*               InputManager::TouchStates;

vector<InputPlayer> InputManager::Players;
vector<InputAction> InputManager::Actions;

static std::map<std::string, Uint16> KeymodStrToFlags;

struct TouchState {
    float X;
    float Y;
    bool  Down;
    bool  Pressed;
    bool  Released;
};

void  InputManager::Init() {
    memset(KeyboardState, 0, NUM_KEYBOARD_KEYS);
    memset(KeyboardStateLast, 0, NUM_KEYBOARD_KEYS);

    KeymodState = 0;

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

void InputManager::InitStringLookup() {
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

    KeymodStrToFlags.insert({ "shift",    KB_MODIFIER_SHIFT });
    KeymodStrToFlags.insert({ "ctrl",     KB_MODIFIER_CTRL });
    KeymodStrToFlags.insert({ "alt",      KB_MODIFIER_ALT });
    KeymodStrToFlags.insert({ "lshift",   KB_MODIFIER_LSHIFT });
    KeymodStrToFlags.insert({ "rshift",   KB_MODIFIER_RSHIFT });
    KeymodStrToFlags.insert({ "lctrl",    KB_MODIFIER_LCTRL });
    KeymodStrToFlags.insert({ "rctrl",    KB_MODIFIER_RCTRL });
    KeymodStrToFlags.insert({ "lalt",     KB_MODIFIER_LALT });
    KeymodStrToFlags.insert({ "ralt",     KB_MODIFIER_RALT });
    KeymodStrToFlags.insert({ "numlock",  KB_MODIFIER_NUM });
    KeymodStrToFlags.insert({ "capslock", KB_MODIFIER_CAPS });
}

char* InputManager::GetKeyName(int key) {
    if (!NameMap::Keys->Exists(key))
        return nullptr;
    return (char*)NameMap::Keys->Get(key);
}
char* InputManager::GetButtonName(int button) {
    if (!NameMap::Buttons->Exists(button))
        return nullptr;
    return (char*)NameMap::Buttons->Get(button);
}
char* InputManager::GetAxisName(int axis) {
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

int InputManager::ParseKeyName(const char* key) {
    FIND_IN_BIJECTIVE(NameMap::Keys, key);
}
int InputManager::ParseButtonName(const char* button) {
    FIND_IN_BIJECTIVE(NameMap::Buttons, button);
}
int InputManager::ParseAxisName(const char* axis) {
    FIND_IN_BIJECTIVE(NameMap::Axes, axis);
}

#undef FIND_IN_BIJECTIVE

Controller* InputManager::OpenController(int index) {
    Controller* controller = new Controller(index);
    if (controller->Device == nullptr) {
        Log::Print(Log::LOG_ERROR, "Opening controller %d failed: %s", index, SDL_GetError());
        delete controller;
    }

    Log::Print(Log::LOG_VERBOSE, "Controller %d opened", index);

    return controller;
}

void  InputManager::InitControllers() {
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

int InputManager::FindController(int joystickID) {
    for (int i = 0; i < InputManager::NumControllers; i++) {
        Controller* controller = InputManager::Controllers[i];
        if (controller->Connected && controller->JoystickID == joystickID)
            return i;
    }

    return -1;
}

bool  InputManager::AddController(int index) {
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

void  InputManager::RemoveController(int joystickID) {
    int controller_id = InputManager::FindController(joystickID);
    if (controller_id == -1)
        return;

    InputManager::Controllers[controller_id]->Close();
}

void  InputManager::Poll() {
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

    SDL_Keymod sdlKeyMod = SDL_GetModState();

    KeymodState = 0;

    if (sdlKeyMod & KMOD_LSHIFT)
        KeymodState |= KB_MODIFIER_LSHIFT | KB_MODIFIER_SHIFT;
    if (sdlKeyMod & KMOD_RSHIFT)
        KeymodState |= KB_MODIFIER_RSHIFT | KB_MODIFIER_SHIFT;
    if (sdlKeyMod & KMOD_LCTRL)
        KeymodState |= KB_MODIFIER_LCTRL | KB_MODIFIER_CTRL;
    if (sdlKeyMod & KMOD_RCTRL)
        KeymodState |= KB_MODIFIER_RCTRL | KB_MODIFIER_CTRL;
    if (sdlKeyMod & KMOD_LALT)
        KeymodState |= KB_MODIFIER_LALT | KB_MODIFIER_ALT;
    if (sdlKeyMod & KMOD_RALT)
        KeymodState |= KB_MODIFIER_RALT | KB_MODIFIER_ALT;
    if (sdlKeyMod & KMOD_NUM)
        KeymodState |= KB_MODIFIER_NUM;
    if (sdlKeyMod & KMOD_CAPS)
        KeymodState |= KB_MODIFIER_CAPS;

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

Uint16 InputManager::CheckKeyModifiers(Uint16 modifiers) {
    return (KeymodState & modifiers) == modifiers;
}

bool  InputManager::IsKeyDown(int key) {
    int scancode = (int)KeyToSDLScancode[key];
    return KeyboardState[scancode];
}
bool  InputManager::IsKeyPressed(int key) {
    int scancode = (int)KeyToSDLScancode[key];
    return KeyboardState[scancode] && !KeyboardStateLast[scancode];
}
bool  InputManager::IsKeyReleased(int key) {
    int scancode = (int)KeyToSDLScancode[key];
    return !KeyboardState[scancode] && KeyboardStateLast[scancode];
}

Controller* InputManager::GetController(int index) {
    if (index >= 0 && index < InputManager::NumControllers) {
        return InputManager::Controllers[index];
    }
    return nullptr;
}

bool  InputManager::ControllerIsConnected(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->Connected;
    return false;
}
bool  InputManager::ControllerIsXbox(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->IsXbox();
    return false;
}
bool  InputManager::ControllerIsPlayStation(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->IsPlayStation();
    return false;
}
bool  InputManager::ControllerIsJoyCon(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->IsJoyCon();
    return false;
}
bool  InputManager::ControllerHasShareButton(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->HasShareButton();
    return false;
}
bool  InputManager::ControllerHasMicrophoneButton(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->HasMicrophoneButton();
    return false;
}
bool  InputManager::ControllerHasPaddles(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->HasPaddles();
    return false;
}
bool  InputManager::ControllerIsButtonHeld(int index, int button) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->IsButtonHeld(button);
    return false;
}
bool  InputManager::ControllerIsButtonPressed(int index, int button) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->IsButtonPressed(button);
    return false;
}
float InputManager::ControllerGetAxis(int index, int axis) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->GetAxis(axis);
    return 0.0f;
}
int   InputManager::ControllerGetType(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return (int)controller->Type;
    return (int)ControllerType::Unknown;
}
char* InputManager::ControllerGetName(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->GetName();
    return nullptr;
}
void  InputManager::ControllerSetPlayerIndex(int index, int player_index) {
    Controller* controller = GetController(index);
    if (controller)
        controller->SetPlayerIndex(player_index);
}
bool  InputManager::ControllerHasRumble(int index) {
    Controller* controller = GetController(index);
    if (controller)
        return controller->Rumble != nullptr;
    return false;
}
bool  InputManager::ControllerIsRumbleActive(int index) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        return controller->Rumble->Active;
    return false;
}
bool  InputManager::ControllerRumble(int index, float large_frequency, float small_frequency, int duration) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        return controller->Rumble->Enable(large_frequency, small_frequency, (Uint32)duration);
    return false;
}
bool  InputManager::ControllerRumble(int index, float strength, int duration) {
    return ControllerRumble(index, strength, strength, duration);
}
void  InputManager::ControllerStopRumble(int index) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        controller->Rumble->Stop();
}
void  InputManager::ControllerStopRumble() {
    for (int i = 0; i < InputManager::NumControllers; i++) {
        Controller* controller = InputManager::Controllers[i];
        if (!controller || !controller->Rumble)
            continue;

        if (controller->Rumble->Active)
            controller->Rumble->Stop();
    }
}
bool  InputManager::ControllerIsRumblePaused(int index) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        return controller->Rumble->Paused;
    return false;
}
void  InputManager::ControllerSetRumblePaused(int index, bool paused) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        controller->Rumble->SetPaused(paused);
}
bool  InputManager::ControllerSetLargeMotorFrequency(int index, float frequency) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        return controller->Rumble->SetLargeMotorFrequency(frequency);
    return false;
}
bool  InputManager::ControllerSetSmallMotorFrequency(int index, float frequency) {
    Controller* controller = GetController(index);
    if (controller && controller->Rumble)
        return controller->Rumble->SetSmallMotorFrequency(frequency);
    return false;
}

float InputManager::TouchGetX(int touch_index) {
    if (touch_index < 0 || touch_index >= NUM_TOUCH_STATES)
        return 0.0f;
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].X;
}
float InputManager::TouchGetY(int touch_index) {
    if (touch_index < 0 || touch_index >= NUM_TOUCH_STATES)
        return 0.0f;
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Y;
}
bool  InputManager::TouchIsDown(int touch_index) {
    if (touch_index < 0 || touch_index >= NUM_TOUCH_STATES)
        return false;
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Down;
}
bool  InputManager::TouchIsPressed(int touch_index) {
    if (touch_index < 0 || touch_index >= NUM_TOUCH_STATES)
        return false;
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Pressed;
}
bool  InputManager::TouchIsReleased(int touch_index) {
    if (touch_index < 0 || touch_index >= NUM_TOUCH_STATES)
        return false;
    TouchState* states = (TouchState*)TouchStates;
    return states[touch_index].Released;
}

int   InputManager::AddPlayer() {
    int id = (int)Players.size();

    InputPlayer player(id);

    player.SetNumActions(Actions.size());

    Players.push_back(player);

    return id;
}
int   InputManager::GetPlayerCount() {
    return (int)Players.size();
}
void  InputManager::SetPlayerControllerIndex(unsigned playerID, int index) {
    if (playerID >= Players.size())
        return;

    if (index < 0)
        index = -1;

    Players[playerID].ControllerIndex = index;
}
int   InputManager::GetPlayerControllerIndex(unsigned playerID) {
    if (playerID >= Players.size())
        return -1;

    return Players[playerID].ControllerIndex;
}
bool  InputManager::IsActionHeld(unsigned playerID, unsigned actionID) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsInputHeld(actionID);
}
bool  InputManager::IsActionPressed(unsigned playerID, unsigned actionID) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsInputPressed(actionID);
}
bool  InputManager::IsActionReleased(unsigned playerID, unsigned actionID) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsInputReleased(actionID);
}
bool  InputManager::IsAnyActionHeld(unsigned playerID) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsAnyInputHeld();
}
bool  InputManager::IsAnyActionPressed(unsigned playerID) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsAnyInputPressed();
}
bool  InputManager::IsAnyActionReleased(unsigned playerID) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsAnyInputReleased();
}
bool  InputManager::IsActionHeld(unsigned playerID, unsigned actionID, unsigned device) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsInputHeld(actionID, device);
}
bool  InputManager::IsActionPressed(unsigned playerID, unsigned actionID, unsigned device) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsInputPressed(actionID, device);
}
bool  InputManager::IsActionReleased(unsigned playerID, unsigned actionID, unsigned device) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsInputReleased(actionID, device);
}
bool  InputManager::IsAnyActionHeld(unsigned playerID, unsigned device) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsAnyInputHeld(device);
}
bool  InputManager::IsAnyActionPressed(unsigned playerID, unsigned device) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsAnyInputPressed(device);
}
bool  InputManager::IsAnyActionReleased(unsigned playerID, unsigned device) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsAnyInputReleased(device);
}
bool  InputManager::IsPlayerUsingDevice(unsigned playerID, unsigned device) {
    if (playerID >= Players.size() || device >= InputDevice_MAX)
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsUsingDevice[device];
}
float InputManager::GetAnalogActionInput(unsigned playerID, unsigned actionID) {
    if (playerID >= Players.size())
        return 0.0f;

    InputPlayer& player = Players[playerID];

    return player.GetAnalogActionInput(actionID);
}

InputBind* InputManager::GetPlayerInputBind(unsigned playerID, unsigned actionID, unsigned index, bool isDefault) {
    if (playerID >= Players.size())
        return nullptr;

    InputPlayer& player = Players[playerID];

    if (isDefault)
        return player.GetDefaultBind(actionID, index);
    else
        return player.GetBind(actionID, index);
}
bool  InputManager::SetPlayerInputBind(unsigned playerID, unsigned actionID, InputBind* bind, unsigned index, bool isDefault) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    if (isDefault)
        return player.ReplaceDefaultBind(actionID, bind, index);
    else
        return player.ReplaceBind(actionID, bind, index);
}
int   InputManager::AddPlayerInputBind(unsigned playerID, unsigned actionID, InputBind* bind, bool isDefault) {
    if (playerID >= Players.size())
        return -1;

    InputPlayer& player = Players[playerID];

    if (isDefault)
        return player.AddDefaultBind(actionID, bind);
    else
        return player.AddBind(actionID, bind);
}
bool  InputManager::RemovePlayerInputBind(unsigned playerID, unsigned actionID, unsigned index, bool isDefault) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    if (isDefault)
        return player.RemoveDefaultBind(actionID, index);
    else
        return player.RemoveBind(actionID, index);
}
int InputManager::GetPlayerInputBindCount(unsigned playerID, unsigned actionID, bool isDefault) {
    if (playerID >= Players.size())
        return 0;

    InputPlayer& player = Players[playerID];

    if (isDefault)
        return player.GetDefaultBindCount(actionID);
    else
        return player.GetBindCount(actionID);
}

void  InputManager::ClearPlayerBinds(unsigned playerID, unsigned actionID, bool isDefault) {
    if (playerID >= Players.size())
        return;

    InputPlayer& player = Players[playerID];

    if (isDefault)
        player.ClearDefaultBinds(actionID);
    else
        player.ClearBinds(actionID);
}

bool  InputManager::IsBindIndexValid(unsigned playerID, unsigned actionID, unsigned index) {
    if (playerID >= Players.size())
        return false;

    InputPlayer& player = Players[playerID];

    return player.IsBindIndexValid(actionID, index);
}

void  InputManager::ResetPlayerBinds(unsigned playerID) {
    if (playerID >= Players.size())
        return;

    InputPlayer& player = Players[playerID];

    player.ResetBinds();
}

void  InputManager::ClearPlayers() {
    Players.clear();
}

int   InputManager::RegisterAction(const char* name) {
    int id = GetActionID(name);
    if (id != -1)
        return id;

    id = (int)Actions.size();

    InputAction input(name, id);

    Actions.push_back(input);

    for (size_t i = 0; i < Players.size(); i++)
        Players[i].SetNumActions(Actions.size());

    return id;
}
int   InputManager::GetActionID(const char* name) {
    if (name != nullptr && name[0] != '\0') {
        for (size_t i = 0; i < Actions.size(); i++) {
            if (strcmp(Actions[i].Name.c_str(), name) == 0)
                return (int)i;
        }
    }

    return -1;
}
void  InputManager::ClearInputs() {
    Actions.clear();

    for (size_t i = 0; i < Players.size(); i++)
        Players[i].SetNumActions(0);
}

void  InputManager::InitPlayerControls() {
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
        AddPlayer();

    // Parse inputs
    for (size_t i = 0; i < controlsNode->children.size(); i++) {
        XMLNode* child = controlsNode->children[i];
        if (XMLParser::MatchToken(child->name, "action")) {
            if (child->children[0]->name.Length > 0) {
                Token& name = child->children[0]->name;
                RegisterAction(name.ToString().c_str());
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

            InputPlayer& player = Players[id - 1];

            player.ControllerIndex = id - 1;

            ParsePlayerControls(player, child);

            player.ResetBinds();
        }
    }
}

void InputManager::ParsePlayerControls(InputPlayer& player, XMLNode* node) {
    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (XMLParser::MatchToken(child->name, "default")) {
            if (child->attributes.Exists("action")) {
                std::string actionName = child->attributes.Get("action").ToString();
                int actionID = GetActionID(actionName.c_str());
                if (actionID != -1) {
                    ParseDefaultInputBinds(player, actionID, actionName, child);
                }
            }
            else if (child->attributes.Exists("copy") && child->attributes.Exists("id")) {
                int id = 0;
                if (!StringUtils::ToNumber(&id, child->attributes.Get("id").ToString()))
                    continue;

                if (id < 1 || id > GetPlayerCount())
                    continue;

                std::string copyType = child->attributes.Get("copy").ToString();
                int filterType = -1;
                if (copyType == "key") {
                    filterType = INPUT_BIND_KEYBOARD;
                }
                else if (copyType == "button") {
                    filterType = INPUT_BIND_CONTROLLER_BUTTON;
                }
                else if (copyType == "axis") {
                    filterType = INPUT_BIND_CONTROLLER_AXIS;
                }
                else {
                    continue;
                }

                InputPlayer& copyPlayer = Players[id - 1];
                player.CopyDefaultBinds(copyPlayer, filterType);
            }
        }
    }
}

Uint16 InputManager::ParseKeyModifiers(string& str, string& actionName) {
    Uint16 flags = 0;

    std::stringstream strStream(str);
    std::string token;
    std::vector<std::string> result;

    while (getline(strStream, token, ' ')) {
        result.push_back(token);
    }

    for (int i = 0; i < result.size(); i++) {
        token = result[i];

        auto it = KeymodStrToFlags.find(token);
        if (it != KeymodStrToFlags.end()) {
            flags |= it->second;
        }
        else {
            Log::Print(Log::LOG_ERROR, "Unknown key modifier \"%s\" for action \"%s\"!", token.c_str(), actionName.c_str());
        }
    }

    return flags;
}

void InputManager::ParseDefaultInputBinds(InputPlayer& player, int actionID, string& actionName, XMLNode* node) {
    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];

        if (XMLParser::MatchToken(child->name, "key")) {
            std::string keyNameStr = child->children[0]->name.ToString();
            const char* keyName = keyNameStr.c_str();
            int key = ParseKeyName(keyName);
            if (key != Key_UNKNOWN) {
                KeyboardBind* bind = new KeyboardBind(key);

                if (child->attributes.Exists("modifiers")) {
                    std::string keymod = child->attributes.Get("modifiers").ToString();
                    bind->Modifiers = ParseKeyModifiers(keymod, actionName);
                }

                if (player.AddDefaultBind(actionID, bind) < 0)
                    delete bind;
            }
            else {
                Log::Print(Log::LOG_ERROR, "Unknown key \"%s\" for action \"%s\"!", keyName, actionName.c_str());
            }
        }
        else if (XMLParser::MatchToken(child->name, "button")) {
            std::string buttonNameStr = child->children[0]->name.ToString();
            const char* buttonName = buttonNameStr.c_str();
            int button = ParseButtonName(buttonName);
            if (button != -1) {
                ControllerButtonBind* bind = new ControllerButtonBind(button);
                if (player.AddDefaultBind(actionID, bind) < 0)
                    delete bind;
            }
            else {
                Log::Print(Log::LOG_ERROR, "Unknown controller button \"%s\" for action \"%s\"!", buttonName, actionName.c_str());
            }
        }
        else if (XMLParser::MatchToken(child->name, "axis")) {
            std::string axisNameStr = child->children[0]->name.ToString();
            const char* axisName = axisNameStr.c_str();

            int axisID;
            if (axisName[0] == '-' || axisName[0] == '+')
                axisID = ParseAxisName(&axisName[1]);
            else
                axisID = ParseAxisName(axisName);

            if (axisID == -1) {
                Log::Print(Log::LOG_ERROR, "Unknown controller axis \"%s\" for action \"%s\"!", axisName, actionName.c_str());
                continue;
            }

            ControllerAxisBind* bind = new ControllerAxisBind();
            bind->Axis = axisID;
            bind->IsAxisNegative = axisName[0] == '-';

            if (child->attributes.Exists("deadzone")) {
                double deadzone;
                if (StringUtils::ToDecimal(&deadzone, child->attributes.Get("deadzone").ToString())) {
                    bind->AxisDeadzone = deadzone;
                }
            }

            if (child->attributes.Exists("digital_threshold")) {
                double threshold;
                if (StringUtils::ToDecimal(&threshold, child->attributes.Get("digital_threshold").ToString())) {
                    bind->AxisDigitalThreshold = threshold;
                }
            }

            if (player.AddDefaultBind(actionID, bind) < 0)
                delete bind;
        }
    }
}

void  InputManager::Dispose() {
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
