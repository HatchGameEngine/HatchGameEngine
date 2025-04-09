#ifndef ENGINE_INPUTMANAGER_H
#define ENGINE_INPUTMANAGER_H

#include <Engine/Application.h>
#include <Engine/Includes/BijectiveMap.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Input/Controller.h>
#include <Engine/Input/ControllerRumble.h>
#include <Engine/Input/Input.h>
#include <Engine/Input/InputAction.h>
#include <Engine/Input/InputPlayer.h>
#include <Engine/TextFormats/XML/XMLNode.h>
#include <Engine/TextFormats/XML/XMLParser.h>

namespace InputManager {
//private:
	void InitStringLookup();
	int FindController(int joystickID);
	void ParsePlayerControls(InputPlayer& player, XMLNode* node);
	Uint16 ParseKeyModifiers(string& str, string& actionName);
	void ParseDefaultInputBinds(InputPlayer& player,
		int actionID,
		string& actionName,
		XMLNode* node);

//public:
	extern float MouseX;
	extern float MouseY;
	extern int MouseDown;
	extern int MousePressed;
	extern int MouseReleased;
	extern Uint8 KeyboardState[0x120];
	extern Uint8 KeyboardStateLast[0x120];
	extern Uint16 KeymodState;
	extern SDL_Scancode KeyToSDLScancode[NUM_KEYBOARD_KEYS];
	extern int NumControllers;
	extern vector<Controller*> Controllers;
	extern SDL_TouchID TouchDevice;
	extern void* TouchStates;
	extern vector<InputPlayer> Players;
	extern vector<InputAction> Actions;

	void Init();
	char* GetKeyName(int key);
	char* GetButtonName(int button);
	char* GetAxisName(int axis);
	int ParseKeyName(const char* key);
	int ParseButtonName(const char* button);
	int ParseAxisName(const char* axis);
	Controller* OpenController(int index);
	void InitControllers();
	bool AddController(int index);
	void RemoveController(int joystickID);
	void Poll();
	Uint16 CheckKeyModifiers(Uint16 modifiers);
	bool IsKeyDown(int key);
	bool IsKeyPressed(int key);
	bool IsKeyReleased(int key);
	Controller* GetController(int index);
	bool ControllerIsConnected(int index);
	bool ControllerIsXbox(int index);
	bool ControllerIsPlayStation(int index);
	bool ControllerIsJoyCon(int index);
	bool ControllerHasShareButton(int index);
	bool ControllerHasMicrophoneButton(int index);
	bool ControllerHasPaddles(int index);
	bool ControllerIsButtonHeld(int index, int button);
	bool ControllerIsButtonPressed(int index, int button);
	float ControllerGetAxis(int index, int axis);
	int ControllerGetType(int index);
	char* ControllerGetName(int index);
	void ControllerSetPlayerIndex(int index, int player_index);
	bool ControllerHasRumble(int index);
	bool ControllerIsRumbleActive(int index);
	bool
	ControllerRumble(int index, float large_frequency, float small_frequency, int duration);
	bool ControllerRumble(int index, float strength, int duration);
	void ControllerStopRumble(int index);
	void ControllerStopRumble();
	bool ControllerIsRumblePaused(int index);
	void ControllerSetRumblePaused(int index, bool paused);
	bool ControllerSetLargeMotorFrequency(int index, float frequency);
	bool ControllerSetSmallMotorFrequency(int index, float frequency);
	float TouchGetX(int touch_index);
	float TouchGetY(int touch_index);
	bool TouchIsDown(int touch_index);
	bool TouchIsPressed(int touch_index);
	bool TouchIsReleased(int touch_index);
	int AddPlayer();
	int GetPlayerCount();
	void SetPlayerControllerIndex(unsigned playerID, int index);
	int GetPlayerControllerIndex(unsigned playerID);
	bool IsActionHeld(unsigned playerID, unsigned actionID);
	bool IsActionPressed(unsigned playerID, unsigned actionID);
	bool IsActionReleased(unsigned playerID, unsigned actionID);
	bool IsAnyActionHeld(unsigned playerID);
	bool IsAnyActionPressed(unsigned playerID);
	bool IsAnyActionReleased(unsigned playerID);
	bool IsActionHeld(unsigned playerID, unsigned actionID, unsigned device);
	bool IsActionPressed(unsigned playerID, unsigned actionID, unsigned device);
	bool IsActionReleased(unsigned playerID, unsigned actionID, unsigned device);
	bool IsAnyActionHeld(unsigned playerID, unsigned device);
	bool IsAnyActionPressed(unsigned playerID, unsigned device);
	bool IsAnyActionReleased(unsigned playerID, unsigned device);
	bool IsPlayerUsingDevice(unsigned playerID, unsigned device);
	float GetAnalogActionInput(unsigned playerID, unsigned actionID);
	InputBind*
	GetPlayerInputBind(unsigned playerID, unsigned actionID, unsigned index, bool isDefault);
	bool SetPlayerInputBind(unsigned playerID,
		unsigned actionID,
		InputBind* bind,
		unsigned index,
		bool isDefault);
	int
	AddPlayerInputBind(unsigned playerID, unsigned actionID, InputBind* bind, bool isDefault);
	bool
	RemovePlayerInputBind(unsigned playerID, unsigned actionID, unsigned index, bool isDefault);
	int GetPlayerInputBindCount(unsigned playerID, unsigned actionID, bool isDefault);
	void ClearPlayerBinds(unsigned playerID, unsigned actionID, bool isDefault);
	bool IsBindIndexValid(unsigned playerID, unsigned actionID, unsigned index);
	void ResetPlayerBinds(unsigned playerID);
	void ClearPlayers();
	int RegisterAction(const char* name);
	int GetActionID(const char* name);
	void ClearInputs();
	void InitPlayerControls();
	void Dispose();
};

#endif /* ENGINE_INPUTMANAGER_H */
