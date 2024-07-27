#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Input/Input.h>
#include <Engine/Input/InputAction.h>

class InputPlayer {
public:
    int ID;

    vector<PlayerInputConfig> Binds;
    vector<PlayerInputConfig> DefaultBinds;

    PlayerInputStatus Status[2];
    PlayerInputStatus AllStatus;

    bool IsUsingDevice[2];

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

    for (size_t n = oldNum; n < num; n++) {
        Binds[n].Clear();
        DefaultBinds[n].Clear();
    }

    for (size_t i = 0; i < 2; i++)
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

    for (size_t s = 0; s < 2; s++) {
        Status[s].AnyHeld = false;
        Status[s].AnyPressed = false;

        for (size_t i = 0; i < Binds.size(); i++) {
            bool isHeld = CheckIfInputHeld(i, s);
            bool isPressed = CheckIfInputPressed(i, s);

            if (isHeld) {
                Status[s].AnyHeld = true;
                AllStatus.AnyHeld = true;
                AllStatus.Held[i] = true;
            }

            if (isPressed) {
                Status[s].AnyPressed = true;
                AllStatus.AnyPressed = true;
                AllStatus.Pressed[i] = true;
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
PUBLIC void InputPlayer::SetControllerBind(unsigned num, struct ControllerBind bind) {
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
        if (bind != -1 && InputManager::IsKeyPressed(bind))
            return true;
    }
    else if (device == InputDevice_Controller && IsControllerBindPressed(actionID))
        return true;

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
PUBLIC bool InputPlayer::IsAnyInputHeld() {
    return AllStatus.AnyHeld;
}
PUBLIC bool InputPlayer::IsAnyInputPressed() {
    return AllStatus.AnyPressed;
}

PUBLIC float InputPlayer::GetAnalogActionInput(unsigned actionID) {
    if (actionID >= InputManager::Actions.size())
        return false;

    float result = 0.0f;

    if (!GetAnalogControllerBind(actionID, &result)) {
        if (IsInputHeld(actionID))
            result = 1.0f;
    }

    return result;
}

PRIVATE bool InputPlayer::IsControllerBindHeld(unsigned num) {
    if (ControllerIndex == -1)
        return false;

    Controller* controller = InputManager::GetController(ControllerIndex);
    if (controller == nullptr)
        return false;

    ControllerBind& bind = Binds[num].ControllerBind;

    bool isHeld = false;
    if (bind.Axis != -1) {
        if (bind.IsAxisNegative) {
            if (controller->GetAxis(bind.Axis) < -bind.AxisDigitalThreshold)
                isHeld = true;
        }
        else if (controller->GetAxis(bind.Axis) > bind.AxisDigitalThreshold)
            isHeld = true;
    }
    if (bind.Button != -1 && controller->IsButtonHeld(bind.Button))
        isHeld = true;

    return isHeld;
}
PRIVATE bool InputPlayer::IsControllerBindPressed(unsigned num) {
    if (ControllerIndex == -1)
        return false;

    Controller* controller = InputManager::GetController(ControllerIndex);
    if (controller == nullptr)
        return false;

    ControllerBind& bind = Binds[num].ControllerBind;

    bool isPressed = false;
    if (bind.Button != -1 && controller->IsButtonPressed(bind.Button))
        isPressed = true;

    return isPressed;
}
PRIVATE bool InputPlayer::GetAnalogControllerBind(unsigned num, float* result) {
    if (ControllerIndex == -1)
        return false;

    Controller* controller = InputManager::GetController(ControllerIndex);
    if (controller == nullptr)
        return false;

    ControllerBind& bind = Binds[num].ControllerBind;
    if (bind.Axis != -1) {
        *result = controller->GetAxis(bind.Axis);
        return true;
    }
    else if (bind.Button != -1 && controller->IsButtonHeld(bind.Button)) {
        *result = 1.0f;
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
PUBLIC void InputPlayer::SetDefaultControllerBind(unsigned num, struct ControllerBind bind) {
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
