#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Input/Input.h>
#include <Engine/Input/InputAction.h>

class InputPlayer {
public:
    int ID;

    vector<PlayerInputConfig> Binds;
    vector<PlayerInputConfig> DefaultBinds;

    PlayerInputStatus Status[InputDevice_MAX];
    PlayerInputStatus AllStatus;

    vector<Uint8> ControllerState;

    bool IsUsingDevice[InputDevice_MAX];

    int ControllerIndex = -1;

    const char* ConfigFilename = nullptr;
};
#endif

#include <Engine/Input/InputPlayer.h>
#include <Engine/Input/Controller.h>
#include <Engine/InputManager.h>

PUBLIC      InputPlayer::InputPlayer(int id) {
    ID = id;
}

PUBLIC void InputPlayer::SetNumActions(size_t num) {
    size_t oldNum = Binds.size();

    Binds.resize(num);
    DefaultBinds.resize(num);
    ControllerState.resize(num);

    for (size_t n = oldNum; n < num; n++) {
        Binds[n].Clear();
        DefaultBinds[n].Clear();
        ControllerState[n] = 0;
    }

    for (unsigned i = 0; i < (unsigned)InputDevice_MAX; i++)
        Status[i].SetNumActions(num);

    AllStatus.SetNumActions(num);
}

PUBLIC void InputPlayer::ClearBinds() {
    for (size_t i = 0; i < Binds.size(); i++) {
        Binds[i].Clear();
    }
}
PUBLIC void InputPlayer::ResetBinds() {
    for (size_t i = 0; i < DefaultBinds.size(); i++) {
        Binds[i] = DefaultBinds[i];
    }
}

PUBLIC void InputPlayer::Update() {
    IsUsingDevice[InputDevice_Keyboard] = false;
    IsUsingDevice[InputDevice_Controller] = false;

    AllStatus.Reset();

    for (size_t i = 0; i < Binds.size(); i++)
        InputPlayer::UpdateControllerBind(i);

    for (unsigned s = 0; s < (unsigned)InputDevice_MAX; s++) {
        Status[s].AnyHeld = false;
        Status[s].AnyPressed = false;
        Status[s].AnyReleased = false;

        for (size_t i = 0; i < Binds.size(); i++) {
            bool isHeld = CheckIfInputHeld(i, s);
            bool isPressed = CheckIfInputPressed(i, s);
            bool wasHeld = Status[s].Held[i];

            Status[s].Released[i] = false;

            if (isHeld) {
                Status[s].AnyHeld = true;
                AllStatus.AnyHeld = AllStatus.Held[i] = true;
            }
            else if (wasHeld) {
                Status[s].Released[i] = Status[s].AnyReleased = true;
                AllStatus.AnyReleased = AllStatus.Released[i] = true;
            }

            if (isPressed) {
                Status[s].AnyPressed = true;
                AllStatus.AnyPressed = AllStatus.Pressed[i] = true;
            }

            Status[s].Held[i] = isHeld;
            Status[s].Pressed[i] = isPressed;
        }
    }
}

PUBLIC void InputPlayer::SetKeyboardBind(unsigned num, int bind) {
    if (num < Binds.size())
        Binds[num].KeyboardBind = bind;
}
PUBLIC void InputPlayer::SetControllerBind(unsigned num, ControllerBind bind) {
    if (num < Binds.size())
        Binds[num].ControllerBind = bind;
}
PUBLIC void InputPlayer::SetControllerBindAxisDeadzone(unsigned num, float deadzone) {
    if (num < Binds.size())
        Binds[num].ControllerBind.AxisDeadzone = deadzone;
}
PUBLIC void InputPlayer::SetControllerBindAxisDigitalThreshold(unsigned num, float threshold) {
    if (num < Binds.size())
        Binds[num].ControllerBind.AxisDigitalThreshold = threshold;
}

PUBLIC void InputPlayer::ClearKeyboardBind(unsigned num) {
    SetKeyboardBind(num, -1);
}
PUBLIC void InputPlayer::ClearControllerBind(unsigned num) {
    if (num < Binds.size())
        Binds[num].ControllerBind.Clear();
}

PUBLIC void InputPlayer::ResetKeyboardBind(unsigned num, int bind) {
    SetKeyboardBind(num, GetDefaultKeyboardBind(num));
}
PUBLIC void InputPlayer::ResetControllerBind(unsigned num, int bind) {
    ControllerBind* defaultBind = GetDefaultControllerBind(num);
    if (defaultBind)
        SetControllerBind(num, *defaultBind);
}

PRIVATE bool InputPlayer::CheckIfInputHeld(unsigned actionID, unsigned device) {
    if (actionID >= InputManager::Actions.size())
        return false;

    if (device == InputDevice_Keyboard) {
        int bind = GetKeyboardBind(actionID);
        if (bind != -1 && InputManager::IsKeyDown(bind)) {
            IsUsingDevice[InputDevice_Keyboard] = true;
            return true;
        }
    }
    else if (device == InputDevice_Controller && IsControllerBindHeld(actionID)) {
        IsUsingDevice[InputDevice_Controller] = true;
        return true;
    }

    return false;
}
PRIVATE bool InputPlayer::CheckIfInputPressed(unsigned actionID, unsigned device) {
    if (actionID >= InputManager::Actions.size())
        return false;

    if (device == InputDevice_Keyboard) {
        int bind = GetKeyboardBind(actionID);
        if (bind != -1 && InputManager::IsKeyPressed(bind)) {
            IsUsingDevice[InputDevice_Keyboard] = true;
            return true;
        }
    }
    else if (device == InputDevice_Controller && IsControllerBindPressed(actionID)) {
        IsUsingDevice[InputDevice_Controller] = true;
        return true;
    }

    return false;
}

PUBLIC bool InputPlayer::IsInputHeld(unsigned actionID, unsigned device) {
    if (actionID >= InputManager::Actions.size() || device >= InputDevice_Controller)
        return false;

    return Status[device].Held[actionID];
}
PUBLIC bool InputPlayer::IsInputPressed(unsigned actionID, unsigned device) {
    if (actionID >= InputManager::Actions.size() || device >= InputDevice_Controller)
        return false;

    return Status[device].Pressed[actionID];
}
PUBLIC bool InputPlayer::IsInputReleased(unsigned actionID, unsigned device) {
    if (actionID >= InputManager::Actions.size() || device >= InputDevice_Controller)
        return false;

    return Status[device].Released[actionID];
}
PUBLIC bool InputPlayer::IsAnyInputHeld(unsigned device) {
    if (device >= InputDevice_Controller)
        return false;

    return Status[device].AnyHeld;
}
PUBLIC bool InputPlayer::IsAnyInputPressed(unsigned device) {
    if (device >= InputDevice_Controller)
        return false;

    return Status[device].AnyPressed;
}
PUBLIC bool InputPlayer::IsAnyInputReleased(unsigned device) {
    if (device >= InputDevice_Controller)
        return false;

    return Status[device].AnyReleased;
}

PUBLIC bool InputPlayer::IsInputHeld(unsigned actionID) {
    if (actionID >= InputManager::Actions.size())
        return false;

    return AllStatus.Held[actionID];
}
PUBLIC bool InputPlayer::IsInputPressed(unsigned actionID) {
    if (actionID >= InputManager::Actions.size())
        return false;

    return AllStatus.Pressed[actionID];
}
PUBLIC bool InputPlayer::IsInputReleased(unsigned actionID) {
    if (actionID >= InputManager::Actions.size())
        return false;

    return AllStatus.Released[actionID];
}
PUBLIC bool InputPlayer::IsAnyInputHeld() {
    return AllStatus.AnyHeld;
}
PUBLIC bool InputPlayer::IsAnyInputPressed() {
    return AllStatus.AnyPressed;
}
PUBLIC bool InputPlayer::IsAnyInputReleased() {
    return AllStatus.AnyReleased;
}

PUBLIC float InputPlayer::GetAnalogActionInput(unsigned actionID) {
    if (actionID >= InputManager::Actions.size())
        return false;

    float result = 0.0f;

    if (!GetAnalogControllerBind(actionID, result)) {
        if (IsInputHeld(actionID))
            result = 1.0f;
    }

    return result;
}

#define BUTTON_UNPUSHED 0
#define BUTTON_PRESSED  1
#define BUTTON_HELD     2
#define BUTTON_RELEASED 3

PRIVATE bool InputPlayer::IsControllerBindHeld(unsigned num) {
    Uint8 state = ControllerState[num];
    return state == BUTTON_PRESSED || state == BUTTON_HELD;
}
PRIVATE bool InputPlayer::IsControllerBindPressed(unsigned num) {
    return ControllerState[num] == BUTTON_PRESSED;
}
PRIVATE void InputPlayer::UpdateControllerBind(unsigned num) {
    Controller* controller = nullptr;
    if (ControllerIndex != -1)
        controller = InputManager::GetController(ControllerIndex);

    PlayerInputStatus &state = Status[InputDevice_Controller];

    bool isDown = false;

    if (controller != nullptr) {
        ControllerBind& bind = Binds[num].ControllerBind;
        if (bind.Axis != -1) {
            if (bind.IsAxisNegative) {
                if (controller->GetAxis(bind.Axis) < -bind.AxisDigitalThreshold)
                    isDown = true;
            }
            else if (controller->GetAxis(bind.Axis) > bind.AxisDigitalThreshold)
                isDown = true;
        }
        if (bind.Button != -1 && controller->IsButtonHeld(bind.Button))
            isDown = true;
    }

    if (isDown) {
        if (ControllerState[num] == BUTTON_RELEASED)
            ControllerState[num] = BUTTON_PRESSED;
        else if (ControllerState[num] < BUTTON_HELD)
            ControllerState[num]++;
    }
    else if (ControllerState[num] == BUTTON_PRESSED || ControllerState[num] == BUTTON_HELD)
        ControllerState[num] = BUTTON_RELEASED;
    else if (ControllerState[num] != BUTTON_UNPUSHED)
        ControllerState[num] = BUTTON_UNPUSHED;
}
PRIVATE bool InputPlayer::GetAnalogControllerBind(unsigned num, float& result) {
    if (ControllerIndex == -1)
        return false;

    Controller* controller = InputManager::GetController(ControllerIndex);
    if (controller == nullptr)
        return false;

    ControllerBind& bind = Binds[num].ControllerBind;
    if (bind.Axis != -1) {
        float magnitude = controller->GetAxis(bind.Axis);

        if (bind.AxisDeadzone != 0.0) {
            if (bind.AxisDeadzone == 1.0) {
                if (magnitude < 1.0)
                    magnitude = 0.0;
            }
            else {
                float scale = (magnitude - bind.AxisDeadzone) / (1.0 - bind.AxisDeadzone);

                if (scale < 0.0)
                    scale = 0.0;
                else if (scale > 1.0)
                    scale = 1.0;

                magnitude *= scale;
            }
        }

        result = magnitude;

        return true;
    }
    else if (bind.Button != -1 && controller->IsButtonHeld(bind.Button)) {
        result = 1.0f;
        return true;
    }

    return false;
}

PUBLIC int  InputPlayer::GetKeyboardBind(unsigned num) {
    if (num < Binds.size())
        return Binds[num].KeyboardBind;

    return -1;
}
PUBLIC ControllerBind* InputPlayer::GetControllerBind(unsigned num) {
    if (num < Binds.size())
        return &Binds[num].ControllerBind;

    return nullptr;
}

PUBLIC void InputPlayer::SetDefaultKeyboardBind(unsigned num, int bind) {
    if (num < DefaultBinds.size())
        DefaultBinds[num].KeyboardBind = bind;
}
PUBLIC void InputPlayer::SetDefaultControllerBind(unsigned num, ControllerBind bind) {
    if (num < DefaultBinds.size())
        DefaultBinds[num].ControllerBind = bind;
}

PUBLIC int  InputPlayer::GetDefaultKeyboardBind(unsigned num) {
    if (num < DefaultBinds.size())
        return DefaultBinds[num].KeyboardBind;

    return -1;
}
PUBLIC ControllerBind* InputPlayer::GetDefaultControllerBind(unsigned num) {
    if (num < DefaultBinds.size())
        return &DefaultBinds[num].ControllerBind;

    return nullptr;
}
