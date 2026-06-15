#ifndef ENGINE_INPUT_CONTROLLER_H
#define ENGINE_INPUT_CONTROLLER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Input/ControllerRumble.h>
#include <Engine/Input/Input.h>

class Controller {
private:
	static ControllerType DetermineType(void* gamecontroller);

public:
	ControllerType Type;
	bool Connected;
	bool* ButtonsPressed;
	bool* ButtonsHeld;
	float* AxisValues;
	ControllerRumble* Rumble;
	SDL_GameController* Device;
	SDL_Joystick* JoystickDevice;
	SDL_JoystickID JoystickID;

	Controller(int index);
	~Controller();
	bool Open(int index);
	void Close();
	void Reset();
	char* GetName();
	void SetPlayerIndex(int index);
	bool GetButton(int button);
	bool IsButtonHeld(int button);
	bool IsButtonPressed(int button);
	float GetAxis(int axis);
	void Update();
	bool IsXbox();
	bool IsPlayStation();
	bool IsJoyCon();
	bool HasShareButton();
	bool HasMicrophoneButton();
	bool HasPaddles();
};

#endif /* ENGINE_INPUT_CONTROLLER_H */
