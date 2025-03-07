#ifndef INPUT_H
#define INPUT_H

#include <Engine/Includes/StandardSDL2.h>

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

enum class ControllerAxis { LeftX, LeftY, RightX, RightY, TriggerLeft, TriggerRight, Max };

#define NUM_INPUT_PLAYERS 8
#define NUM_TOUCH_STATES 8
#define DEFAULT_DIGITAL_AXIS_THRESHOLD 0.5

enum InputDevice { InputDevice_Keyboard, InputDevice_Controller, InputDevice_MAX };

#define KB_MODIFIER_SHIFT 1 // Either Shift key
#define KB_MODIFIER_CTRL 2 // Either Ctrl key
#define KB_MODIFIER_ALT 4 // Either Alt key
#define KB_MODIFIER_LSHIFT 8 // Left Shift
#define KB_MODIFIER_RSHIFT 16 // Right Shift
#define KB_MODIFIER_LCTRL 32 // Left Ctrl
#define KB_MODIFIER_RCTRL 64 // Right Ctrl
#define KB_MODIFIER_LALT 128 // Left Alt
#define KB_MODIFIER_RALT 256 // Right Alt
#define KB_MODIFIER_NUM 512 // Num Lock
#define KB_MODIFIER_CAPS 1024 // Caps Lock

#define INPUT_BIND_KEYBOARD 0
#define INPUT_BIND_CONTROLLER_BUTTON 1
#define INPUT_BIND_CONTROLLER_AXIS 2
#define NUM_INPUT_BIND_TYPES 3

class InputBind {
public:
	Uint8 Type;

	InputBind() { Type = INPUT_BIND_KEYBOARD; }

	InputBind(Uint8 type) { Type = type; }

	virtual void Clear() {}

	virtual bool IsDefined() const = 0;

	virtual InputBind* Clone() const = 0;

	virtual ~InputBind() = default;
};

class KeyboardBind : public InputBind {
public:
	int Key;
	Uint16 Modifiers;

	KeyboardBind() : InputBind(INPUT_BIND_KEYBOARD) { Clear(); }

	KeyboardBind(int key) : KeyboardBind() { Key = key; }

	void Clear() {
		Key = Key_UNKNOWN;
		Modifiers = 0;
	}

	bool IsDefined() const { return Key != Key_UNKNOWN; }

	InputBind* Clone() const {
		KeyboardBind* clone = new KeyboardBind(Key);
		clone->Modifiers = Modifiers;
		return static_cast<InputBind*>(clone);
	}
};

class ControllerButtonBind : public InputBind {
public:
	int Button;

	ControllerButtonBind() : InputBind(INPUT_BIND_CONTROLLER_BUTTON) { Clear(); }

	ControllerButtonBind(int button) : ControllerButtonBind() { Button = button; }

	void Clear() { Button = -1; }

	bool IsDefined() const { return Button != -1; }

	InputBind* Clone() const {
		ControllerButtonBind* clone = new ControllerButtonBind(Button);
		return static_cast<InputBind*>(clone);
	}
};

class ControllerAxisBind : public InputBind {
public:
	int Axis;
	double AxisDeadzone;
	double AxisDigitalThreshold;
	bool IsAxisNegative;

	ControllerAxisBind() : InputBind(INPUT_BIND_CONTROLLER_AXIS) { Clear(); }

	ControllerAxisBind(int axis) : ControllerAxisBind() { Axis = axis; }

	void Clear() {
		Axis = -1;
		AxisDeadzone = 0.0;
		AxisDigitalThreshold = DEFAULT_DIGITAL_AXIS_THRESHOLD;
		IsAxisNegative = false;
	}

	bool IsDefined() const { return Axis != -1; }

	InputBind* Clone() const {
		ControllerAxisBind* clone = new ControllerAxisBind(Axis);
		clone->AxisDeadzone = AxisDeadzone;
		clone->AxisDigitalThreshold = AxisDigitalThreshold;
		clone->IsAxisNegative = IsAxisNegative;
		return static_cast<InputBind*>(clone);
	}
};

#define INPUT_STATE_UNPUSHED 0
#define INPUT_STATE_PRESSED 1
#define INPUT_STATE_HELD 2
#define INPUT_STATE_RELEASED 3

struct PlayerInputStatus {
	vector<Uint8> State;

	Uint8 NumHeld;
	Uint8 NumPressed;
	Uint8 NumReleased;

	void SetNumActions(size_t num) {
		size_t oldNum = State.size();

		State.resize(num);

		for (size_t i = oldNum; i < num; i++) {
			State[i] = INPUT_STATE_UNPUSHED;
		}
	}

	void Reset() {
		NumHeld = NumPressed = NumReleased = 0;

		for (size_t i = 0; i < State.size(); i++) {
			State[i] = INPUT_STATE_UNPUSHED;
		}
	}
};

struct PlayerInputConfig {
	std::vector<InputBind*> Binds;

	PlayerInputConfig() { Binds.clear(); }

	void Clear() {
		for (size_t i = 0; i < Binds.size(); i++) {
			delete Binds[i];
		}

		Binds.clear();
	}

	~PlayerInputConfig() { Clear(); }
};

#endif /* INPUT_H */
