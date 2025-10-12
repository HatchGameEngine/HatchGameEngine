#ifndef ENGINE_INPUT_CONTROLLER_H
#define ENGINE_INPUT_CONTROLLER_H

#include <Engine/Includes/AppEvent.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Input/ControllerRumble.h>
#include <Engine/Input/Input.h>

class Controller {
private:
	static ControllerType DetermineType(void* gamecontroller);

public:
	ControllerType Type;
	bool Connected = false;
	bool* ButtonsPressed = nullptr;
	bool* ButtonsHeld = nullptr;
	bool* ButtonsHeldLast = nullptr;
	float* AxisValues = nullptr;
	ControllerRumble* Rumble = nullptr;
	SDL_GameController* Device = nullptr;
	SDL_Joystick* JoystickDevice = nullptr;
	SDL_JoystickID JoystickID = 0;

	Controller(int joystickID);
	~Controller();
	bool Open(int joystickID);
	void Close();
	void Reset();
	char* GetName();
	void SetPlayerIndex(int index);
	int RemapButton(int button);
	bool IsButtonHeld(int button);
	bool IsButtonPressed(int button);
	float GetAxis(int axis);
	void SetLastState();
	void RespondToEvent(AppEvent& event);
	void Update();
	bool IsXbox();
	bool IsPlayStation();
	bool IsJoyCon();
	bool HasShareButton();
	bool HasMicrophoneButton();
	bool HasPaddles();
};

#endif /* ENGINE_INPUT_CONTROLLER_H */
