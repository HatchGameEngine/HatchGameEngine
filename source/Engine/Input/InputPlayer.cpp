#include <Engine/Input/Controller.h>
#include <Engine/Input/InputPlayer.h>
#include <Engine/InputManager.h>

InputPlayer::InputPlayer(int id) {
	ID = id;
}

void InputPlayer::SetNumActions(size_t num) {
	size_t oldNum = Binds.size();

	Binds.resize(num);
	DefaultBinds.resize(num);

	NumHeld.resize(num);
	NumPressed.resize(num);
	NumReleased.resize(num);

	ControllerState.resize(num);

	for (size_t n = oldNum; n < num; n++) {
		Binds[n].Clear();
		DefaultBinds[n].Clear();

		NumHeld[n] = NumPressed[n] = NumReleased[n] = 0;

		ControllerState[n] = 0;
	}

	for (unsigned i = 0; i < (unsigned)InputDevice_MAX; i++) {
		Status[i].SetNumActions(num);
	}

	AllStatus.SetNumActions(num);
}

void InputPlayer::ClearBinds() {
	for (size_t i = 0; i < Binds.size(); i++) {
		Binds[i].Clear();
	}
}
void InputPlayer::ResetBinds() {
	for (size_t i = 0; i < DefaultBinds.size(); i++) {
		Binds[i].Clear();

		for (size_t j = 0; j < DefaultBinds[i].Binds.size(); j++) {
			InputBind* clone = DefaultBinds[i].Binds[j]->Clone();
			Binds[i].Binds.push_back(clone);
		}
	}
}

void InputPlayer::Update() {
	IsUsingDevice[InputDevice_Keyboard] = false;
	IsUsingDevice[InputDevice_Controller] = false;

	AllStatus.Reset();

	AnyHeld = AnyPressed = AnyReleased = false;

	for (size_t i = 0; i < Binds.size(); i++) {
		NumPressed[i] = NumHeld[i] = NumPressed[i] = 0;
		InputPlayer::UpdateControllerBind(i);
	}

	// Check if inputs are down
	for (unsigned s = 0; s < (unsigned)InputDevice_MAX; s++) {
		Status[s].NumHeld = Status[s].NumPressed = Status[s].NumReleased = 0;

		for (size_t i = 0; i < Binds.size(); i++) {
			bool isHeld = CheckIfInputHeld(i, s);
			bool isPressed = CheckIfInputPressed(i, s);

			Uint8& state = Status[s].State[i];

			if (isPressed) {
				if (state != INPUT_STATE_HELD) {
					state = INPUT_STATE_PRESSED;
					NumPressed[i]++;
				}
				if (AllStatus.State[i] != INPUT_STATE_HELD) {
					AllStatus.State[i] = INPUT_STATE_PRESSED;
				}

				Status[s].NumPressed++;

				AnyPressed = true;
			}
			else if (isHeld) {
				state = AllStatus.State[i] = INPUT_STATE_HELD;
				Status[s].NumHeld++;

				NumHeld[i]++;

				AnyHeld = true;
			}
		}
	}

	// Now check if they were let go
	// This is split in two because an input may be mapped to
	// multiple keys. Consider the following timeline of events,
	// where the same action is mapped to multiple keys or buttons:
	// - Frame #1: Right action is unpushed.
	// - Frame #2: Player holds right arrow on keyboard: Right
	// action is pressed.
	// - Frame #3: Player keeps holding right arrow on keyboard:
	// Right action is held.
	// - Frame #4: Player presses right on the D-Pad: Right action
	// stays held.
	// - Frame #5: Player holds right on the D-Pad: Right action
	// stays held.
	// - Frame #6: Player releases right arrow on keyboard: Right
	// action stays held.
	// - Frame #7: Player releases right on the D-Pad: Right action
	// is released.
	// - Frame #8: Player isn't pressing or holding anything: Right
	// action is unpushed. As you may have observed, the press and
	// release events are not repeated! If any input event keeps
	// the action held, press and release events that are related
	// to the same action do not interfere on the action's held
	// status.
	for (unsigned s = 0; s < (unsigned)InputDevice_MAX; s++) {
		for (size_t i = 0; i < Binds.size(); i++) {
			Uint8& state = Status[s].State[i];

			if (state == INPUT_STATE_PRESSED || state == INPUT_STATE_HELD) {
				if (NumPressed[i] == 0 && NumHeld[i] == 0) {
					state = AllStatus.State[i] = INPUT_STATE_RELEASED;
					Status[s].NumReleased++;

					NumReleased[i]++;

					AnyReleased = true;
				}
			}
			else if (state == INPUT_STATE_RELEASED) {
				state = AllStatus.State[i] = INPUT_STATE_UNPUSHED;
			}
		}
	}
}

size_t InputPlayer::PushBindToList(PlayerInputConfig& config, InputBind* def) {
	config.Binds.push_back(def);

	return config.Binds.size() - 1;
}

bool InputPlayer::ReplaceBindInList(PlayerInputConfig& config, InputBind* def, unsigned index) {
	if (index >= 0 && index < config.Binds.size()) {
		if (config.Binds[index]) {
			delete config.Binds[index];
		}
		config.Binds[index] = def;
		return true;
	}

	return false;
}

bool InputPlayer::RemoveBindFromList(PlayerInputConfig& config, unsigned index) {
	if (index >= 0 && index < config.Binds.size()) {
		config.Binds.erase(config.Binds.begin() + index);
		return true;
	}

	return false;
}

int InputPlayer::AddBind(unsigned num, InputBind* bind) {
	if (num < Binds.size()) {
		return (int)PushBindToList(Binds[num], bind);
	}

	return -1;
}

bool InputPlayer::ReplaceBind(unsigned num, InputBind* bind, unsigned index) {
	if (num < Binds.size()) {
		return ReplaceBindInList(Binds[num], bind, index);
	}
	return false;
}

bool InputPlayer::RemoveBind(unsigned num, unsigned index) {
	if (num < Binds.size()) {
		return RemoveBindFromList(Binds[num], index);
	}
	return false;
}

InputBind* InputPlayer::GetBindAtIndex(PlayerInputConfig& config, unsigned index) {
	if (config.Binds.size() > 0 && index < config.Binds.size()) {
		return config.Binds[index];
	}

	return nullptr;
}

size_t InputPlayer::GetBindCount(PlayerInputConfig& config) {
	return config.Binds.size();
}

InputBind* InputPlayer::GetBind(unsigned num, unsigned index) {
	if (num < Binds.size()) {
		return GetBindAtIndex(Binds[num], index);
	}

	return nullptr;
}

size_t InputPlayer::GetBindCount(unsigned num) {
	if (num < Binds.size()) {
		return GetBindCount(Binds[num]);
	}

	return 0;
}

int InputPlayer::AddDefaultBind(unsigned num, InputBind* bind) {
	if (num < DefaultBinds.size()) {
		return (int)PushBindToList(DefaultBinds[num], bind);
	}

	return -1;
}

void InputPlayer::CopyDefaultBinds(InputPlayer& src, bool filter, int filterType) {
	for (size_t i = 0; i < src.Binds.size(); i++) {
		PlayerInputConfig& config = src.Binds[i];
		for (size_t j = 0; j < config.Binds.size(); j++) {
			InputBind* def = config.Binds[j];
			if (!filter || (def->Type == filterType)) {
				PushBindToList(DefaultBinds[i], def->Clone());
			}
		}
	}
}

void InputPlayer::CopyDefaultBinds(InputPlayer& src) {
	CopyDefaultBinds(src, false, 0);
}

void InputPlayer::CopyDefaultBinds(InputPlayer& src, int filterType) {
	CopyDefaultBinds(src, true, filterType);
}

bool InputPlayer::ReplaceDefaultBind(unsigned num, InputBind* bind, unsigned index) {
	if (num < DefaultBinds.size()) {
		return ReplaceBindInList(DefaultBinds[num], bind, index);
	}
	return false;
}

bool InputPlayer::RemoveDefaultBind(unsigned num, unsigned index) {
	if (num < DefaultBinds.size()) {
		return RemoveBindFromList(DefaultBinds[num], index);
	}
	return false;
}

InputBind* InputPlayer::GetDefaultBind(unsigned num, unsigned index) {
	if (num < DefaultBinds.size()) {
		return GetBindAtIndex(DefaultBinds[num], index);
	}

	return nullptr;
}

size_t InputPlayer::GetDefaultBindCount(unsigned num) {
	if (num < DefaultBinds.size()) {
		return GetBindCount(DefaultBinds[num]);
	}

	return 0;
}

bool InputPlayer::IsBindIndexValid(unsigned num, unsigned index) {
	if (num < Binds.size()) {
		return index < Binds[num].Binds.size();
	}

	return false;
}

void InputPlayer::ClearBinds(unsigned num) {
	if (num < Binds.size()) {
		Binds[num].Binds.clear();
	}
}

void InputPlayer::ClearDefaultBinds(unsigned num) {
	if (num < DefaultBinds.size()) {
		DefaultBinds[num].Binds.clear();
	}
}

void InputPlayer::ResetBindList(PlayerInputConfig& dest, PlayerInputConfig& src) {
	dest.Binds.clear();

	for (size_t i = 0; i < src.Binds.size(); i++) {
		dest.Binds.push_back(src.Binds[i]);
	}
}

void InputPlayer::ResetBind(unsigned num) {
	if (num < Binds.size() && num < DefaultBinds.size()) {
		ResetBindList(Binds[num], DefaultBinds[num]);
	}
}

bool InputPlayer::CheckInputBindState(InputBind* bind, bool held) {
	if (bind->IsDefined() && bind->Type == INPUT_BIND_KEYBOARD) {
		KeyboardBind* keyBind = static_cast<KeyboardBind*>(bind);

		if (keyBind->Modifiers != 0) {
			if (!InputManager::CheckKeyModifiers(keyBind->Modifiers)) {
				return false;
			}
		}

		if (held) {
			return InputManager::IsKeyDown(keyBind->Key);
		}
		else {
			return InputManager::IsKeyPressed(keyBind->Key);
		}
	}

	return false;
}

bool InputPlayer::CheckIfInputHeld(unsigned actionID, unsigned device) {
	if (actionID >= InputManager::Actions.size()) {
		return false;
	}

	if (device == InputDevice_Keyboard && actionID < Binds.size()) {
		for (size_t i = 0; i < Binds[actionID].Binds.size(); i++) {
			InputBind* bind = Binds[actionID].Binds[i];
			if (CheckInputBindState(bind, true)) {
				IsUsingDevice[InputDevice_Keyboard] = true;
				return true;
			}
		}
	}
	else if (device == InputDevice_Controller && IsControllerBindHeld(actionID)) {
		IsUsingDevice[InputDevice_Controller] = true;
		return true;
	}

	return false;
}
bool InputPlayer::CheckIfInputPressed(unsigned actionID, unsigned device) {
	if (actionID >= InputManager::Actions.size()) {
		return false;
	}

	if (device == InputDevice_Keyboard && actionID < Binds.size()) {
		for (size_t i = 0; i < Binds[actionID].Binds.size(); i++) {
			InputBind* bind = Binds[actionID].Binds[i];
			if (CheckInputBindState(bind, false)) {
				IsUsingDevice[InputDevice_Keyboard] = true;
				return true;
			}
		}
	}
	else if (device == InputDevice_Controller && IsControllerBindPressed(actionID)) {
		IsUsingDevice[InputDevice_Controller] = true;
		return true;
	}

	return false;
}

bool InputPlayer::IsInputHeld(unsigned actionID, unsigned device) {
	if (actionID >= InputManager::Actions.size() || device > InputDevice_Controller) {
		return false;
	}

	Uint8 state = Status[device].State[actionID];
	return state == INPUT_STATE_PRESSED || state == INPUT_STATE_HELD;
}
bool InputPlayer::IsInputPressed(unsigned actionID, unsigned device) {
	if (actionID >= InputManager::Actions.size() || device > InputDevice_Controller) {
		return false;
	}

	return Status[device].State[actionID] == INPUT_STATE_PRESSED;
}
bool InputPlayer::IsInputReleased(unsigned actionID, unsigned device) {
	if (actionID >= InputManager::Actions.size() || device > InputDevice_Controller) {
		return false;
	}

	return Status[device].State[actionID] == INPUT_STATE_RELEASED;
}
bool InputPlayer::IsAnyInputHeld(unsigned device) {
	if (device > InputDevice_Controller) {
		return false;
	}

	return Status[device].NumHeld != 0;
}
bool InputPlayer::IsAnyInputPressed(unsigned device) {
	if (device > InputDevice_Controller) {
		return false;
	}

	return Status[device].NumPressed != 0;
}
bool InputPlayer::IsAnyInputReleased(unsigned device) {
	if (device > InputDevice_Controller) {
		return false;
	}

	return Status[device].NumReleased != 0;
}

bool InputPlayer::IsInputHeld(unsigned actionID) {
	if (actionID >= InputManager::Actions.size()) {
		return false;
	}

	Uint8 state = AllStatus.State[actionID];
	return state == INPUT_STATE_PRESSED || state == INPUT_STATE_HELD;
}
bool InputPlayer::IsInputPressed(unsigned actionID) {
	if (actionID >= InputManager::Actions.size()) {
		return false;
	}

	return AllStatus.State[actionID] == INPUT_STATE_PRESSED;
}
bool InputPlayer::IsInputReleased(unsigned actionID) {
	if (actionID >= InputManager::Actions.size()) {
		return false;
	}

	return AllStatus.State[actionID] == INPUT_STATE_RELEASED;
}
bool InputPlayer::IsAnyInputHeld() {
	return AnyHeld;
}
bool InputPlayer::IsAnyInputPressed() {
	return AnyPressed;
}
bool InputPlayer::IsAnyInputReleased() {
	return AnyReleased;
}

float InputPlayer::GetAnalogActionInput(unsigned actionID) {
	if (actionID >= InputManager::Actions.size()) {
		return false;
	}

	float result = 0.0f;

	if (!GetAnalogControllerBind(actionID, result)) {
		if (IsInputHeld(actionID)) {
			result = 1.0f;
		}
	}

	return result;
}

bool InputPlayer::IsControllerBindHeld(unsigned num) {
	Uint8 state = ControllerState[num];
	return state == INPUT_STATE_PRESSED || state == INPUT_STATE_HELD;
}
bool InputPlayer::IsControllerBindPressed(unsigned num) {
	return ControllerState[num] == INPUT_STATE_PRESSED;
}

static float ApplyAxisDeadzone(float magnitude, float deadzone) {
	if (deadzone >= 1.0) {
		if (magnitude < 1.0) {
			return 0.0;
		}
	}

	float scale = (magnitude - deadzone) / (1.0 - deadzone);

	if (scale < 0.0) {
		scale = 0.0;
	}
	else if (scale > 1.0) {
		scale = 1.0;
	}

	return magnitude * scale;
}

static bool CheckControllerBindHeld(PlayerInputConfig& config, Controller* controller) {
	if (controller == nullptr || !controller->Connected) {
		return false;
	}

	for (size_t i = 0; i < config.Binds.size(); i++) {
		InputBind* bind = config.Binds[i];

		if (!bind->IsDefined()) {
			continue;
		}
		else if (bind->Type == INPUT_BIND_CONTROLLER_AXIS) {
			ControllerAxisBind* axisBind = static_cast<ControllerAxisBind*>(bind);

			float magnitude = controller->GetAxis(axisBind->Axis);
			if (axisBind->AxisDeadzone != 0.0) {
				magnitude = ApplyAxisDeadzone(magnitude, axisBind->AxisDeadzone);
			}

			if (axisBind->IsAxisNegative) {
				if (magnitude < -axisBind->AxisDigitalThreshold) {
					return true;
				}
			}
			else if (magnitude > axisBind->AxisDigitalThreshold) {
				return true;
			}
		}
		else if (bind->Type == INPUT_BIND_CONTROLLER_BUTTON) {
			ControllerButtonBind* buttonBind = static_cast<ControllerButtonBind*>(bind);

			if (controller->IsButtonHeld(buttonBind->Button)) {
				return true;
			}
		}
	}

	return false;
}
void InputPlayer::UpdateControllerBind(unsigned num) {
	Controller* controller = InputManager::GetController(ControllerIndex);

	if (CheckControllerBindHeld(Binds[num], controller)) {
		if (ControllerState[num] == INPUT_STATE_RELEASED) {
			ControllerState[num] = INPUT_STATE_PRESSED;
		}
		else if (ControllerState[num] < INPUT_STATE_HELD) {
			ControllerState[num]++;
		}
	}
	else if (ControllerState[num] == INPUT_STATE_PRESSED ||
		ControllerState[num] == INPUT_STATE_HELD) {
		ControllerState[num] = INPUT_STATE_RELEASED;
	}
	else if (ControllerState[num] != INPUT_STATE_UNPUSHED) {
		ControllerState[num] = INPUT_STATE_UNPUSHED;
	}
}
bool InputPlayer::GetAnalogControllerBind(unsigned num, float& result) {
	if (ControllerIndex == -1) {
		return false;
	}

	Controller* controller = InputManager::GetController(ControllerIndex);
	if (controller == nullptr || !controller->Connected) {
		return false;
	}

	for (size_t i = 0; i < Binds[num].Binds.size(); i++) {
		InputBind* bind = Binds[num].Binds[i];
		if (!bind->IsDefined()) {
			continue;
		}
		else if (bind->Type == INPUT_BIND_CONTROLLER_AXIS) {
			ControllerAxisBind* axisBind = static_cast<ControllerAxisBind*>(bind);

			float magnitude = controller->GetAxis(axisBind->Axis);
			if (axisBind->AxisDeadzone != 0.0) {
				result = ApplyAxisDeadzone(magnitude, axisBind->AxisDeadzone);
			}
			else {
				result = magnitude;
			}

			return true;
		}
		else if (bind->Type == INPUT_BIND_CONTROLLER_BUTTON) {
			ControllerButtonBind* buttonBind = static_cast<ControllerButtonBind*>(bind);
			if (controller->IsButtonHeld(buttonBind->Button)) {
				result = 1.0f;
				return true;
			}
		}
	}

	return false;
}
