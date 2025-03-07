#include <Engine/Input/Controller.h>
#include <Engine/InputManager.h>

Controller::Controller(int index) {
	Reset();
	Open(index);
}

Controller::~Controller() {
	Close();
}

#define CONST_BUTTON(x) SDL_CONTROLLER_BUTTON_##x

static SDL_GameControllerButton ButtonEnums[] = {CONST_BUTTON(A),
	CONST_BUTTON(B),
	CONST_BUTTON(X),
	CONST_BUTTON(Y),
	CONST_BUTTON(BACK),
	CONST_BUTTON(GUIDE),
	CONST_BUTTON(START),
	CONST_BUTTON(LEFTSTICK),
	CONST_BUTTON(RIGHTSTICK),
	CONST_BUTTON(LEFTSHOULDER),
	CONST_BUTTON(RIGHTSHOULDER),
	CONST_BUTTON(DPAD_UP),
	CONST_BUTTON(DPAD_DOWN),
	CONST_BUTTON(DPAD_LEFT),
	CONST_BUTTON(DPAD_RIGHT),
	CONST_BUTTON(MISC1),
	CONST_BUTTON(MISC1),
	CONST_BUTTON(TOUCHPAD),
	CONST_BUTTON(PADDLE1),
	CONST_BUTTON(PADDLE2),
	CONST_BUTTON(PADDLE3),
	CONST_BUTTON(PADDLE4),
	CONST_BUTTON(MISC1)};

#undef CONST_BUTTON
#define CONST_AXIS(x) SDL_CONTROLLER_AXIS_##x

static SDL_GameControllerAxis AxisEnums[] = {CONST_AXIS(LEFTX),
	CONST_AXIS(LEFTY),
	CONST_AXIS(RIGHTX),
	CONST_AXIS(RIGHTY),
	CONST_AXIS(TRIGGERLEFT),
	CONST_AXIS(TRIGGERRIGHT)};

#undef CONST_AXIS

ControllerType Controller::DetermineType(void* gamecontroller) {
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
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
		return ControllerType::SwitchPro;
	case SDL_CONTROLLER_TYPE_GOOGLE_STADIA:
		return ControllerType::Stadia;
	case SDL_CONTROLLER_TYPE_AMAZON_LUNA:
		return ControllerType::AmazonLuna;
#if SDL_VERSION_ATLEAST(2, 24, 0)
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
		return ControllerType::SwitchJoyConLeft;
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
		return ControllerType::SwitchJoyConRight;
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
		return ControllerType::SwitchJoyConPair;
	case SDL_CONTROLLER_TYPE_NVIDIA_SHIELD:
		return ControllerType::NvidiaShield;
#endif
	case SDL_CONTROLLER_TYPE_UNKNOWN:
		return ControllerType::Unknown;
	default:
		return ControllerType::Xbox360;
	}
}

bool Controller::Open(int index) {
	Device = SDL_GameControllerOpen(index);
	if (!Device) {
		return false;
	}

	Type = Controller::DetermineType(Device);
	JoystickDevice = SDL_GameControllerGetJoystick(Device);
	JoystickID = SDL_JoystickInstanceID(JoystickDevice);
	Connected = true;

	if (SDL_GameControllerHasRumble(Device)) {
		Rumble = new ControllerRumble(Device);
	}

	ButtonsPressed = (bool*)Memory::Calloc((int)ControllerButton::Max, sizeof(bool));
	ButtonsHeld = (bool*)Memory::Calloc((int)ControllerButton::Max, sizeof(bool));
	AxisValues = (float*)Memory::Calloc((int)ControllerAxis::Max, sizeof(float));

	return true;
}

void Controller::Close() {
	if (Rumble) {
		Rumble->Stop();
		delete Rumble;
		Rumble = nullptr;
	}

	if (ButtonsPressed) {
		Memory::Free(ButtonsPressed);
		ButtonsPressed = nullptr;
	}

	if (ButtonsHeld) {
		Memory::Free(ButtonsHeld);
		ButtonsHeld = nullptr;
	}

	if (AxisValues) {
		Memory::Free(AxisValues);
		AxisValues = nullptr;
	}

	SDL_GameControllerClose(Device);
	Reset();
}

void Controller::Reset() {
	Type = ControllerType::Unknown;
	Connected = false;
	Device = nullptr;
	Rumble = nullptr;
	JoystickDevice = nullptr;
	JoystickID = -1;
}

char* Controller::GetName() {
	return (char*)SDL_GameControllerName(Device);
}
// Sets the LEDs in some controllers
void Controller::SetPlayerIndex(int index) {
	SDL_GameControllerSetPlayerIndex(Device, index);
}

bool Controller::GetButton(int button) {
	if (button >= (int)ControllerButton::Max) {
		return false;
	}
	return SDL_GameControllerGetButton(Device, ButtonEnums[button]);
}

bool Controller::IsButtonHeld(int button) {
	if (button < 0 || button >= (int)ControllerButton::Max) {
		return false;
	}
	return ButtonsHeld[button];
}
bool Controller::IsButtonPressed(int button) {
	if (button < 0 || button >= (int)ControllerButton::Max) {
		return false;
	}
	return ButtonsPressed[button];
}

float Controller::GetAxis(int axis) {
	if (axis < 0 || axis >= (int)ControllerAxis::Max) {
		return 0.0f;
	}
	return AxisValues[axis];
}

void Controller::Update() {
	if (Rumble) {
		Rumble->Update();
	}

	for (unsigned i = 0; i < (unsigned)ControllerButton::Max; i++) {
		bool isDown = GetButton(i);
		ButtonsPressed[i] = !ButtonsHeld[i] && isDown;
		ButtonsHeld[i] = isDown;
	}

	for (unsigned i = 0; i < (unsigned)ControllerAxis::Max; i++) {
		Sint16 value = SDL_GameControllerGetAxis(Device, AxisEnums[i]);
		AxisValues[i] = (float)value / 32767;
	}
}

bool Controller::IsXbox() {
	return Type == ControllerType::Xbox360 || Type == ControllerType::XboxOne ||
		Type == ControllerType::XboxSeriesXS || Type == ControllerType::XboxElite;
}
bool Controller::IsPlayStation() {
	return Type == ControllerType::PS3 || Type == ControllerType::PS4 ||
		Type == ControllerType::PS5;
}
bool Controller::IsJoyCon() {
	return Type == ControllerType::SwitchJoyConLeft ||
		Type == ControllerType::SwitchJoyConRight;
}
bool Controller::HasShareButton() {
	return Type == ControllerType::XboxSeriesXS || Type == ControllerType::SwitchPro ||
		Type == ControllerType::SwitchJoyConLeft;
}
bool Controller::HasMicrophoneButton() {
	return Type == ControllerType::PS5 || Type == ControllerType::AmazonLuna;
}
bool Controller::HasPaddles() {
	return Type == ControllerType::XboxElite;
}
