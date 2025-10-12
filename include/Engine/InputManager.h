#ifndef ENGINE_INPUTMANAGER_H
#define ENGINE_INPUTMANAGER_H

#include <Engine/Application.h>
#include <Engine/Includes/AppEvent.h>
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

class InputManager {
private:
	static void InitLUTs();
	static void ParsePlayerControls(InputPlayer& player, XMLNode* node);
	static Uint16 ParseKeyModifiers(string& str, string& actionName);
	static void ParseDefaultInputBinds(InputPlayer& player,
		int actionID,
		string& actionName,
		XMLNode* node);

public:
	static float MouseX;
	static float MouseY;
	static int MouseDown;
	static int MousePressed;
	static int MouseReleased;
	static int MouseDownLast;
	static int MouseMode;
	static Uint8 KeyboardState[NUM_KEYBOARD_KEYS];
	static Uint8 KeyboardStateLast[NUM_KEYBOARD_KEYS];
	static Uint16 KeymodState;
	static Uint16 SDLScancodeToKey[SDL_NUM_SCANCODES];
	static ControllerButton SDLControllerButtonLookup[(int)ControllerButton::Max];
	static ControllerAxis SDLControllerAxisLookup[(int)ControllerAxis::Max];
	static int NumControllers;
	static vector<Controller*> Controllers;
	static SDL_TouchID TouchDevice;
	static void* TouchStates;
	static vector<InputPlayer> Players;
	static vector<InputAction> Actions;

	static void Init();
	static char* GetKeyName(int key);
	static char* GetButtonName(int button);
	static char* GetAxisName(int axis);
	static int ParseKeyName(const char* key);
	static int ParseButtonName(const char* button);
	static int ParseAxisName(const char* axis);
	static Controller* OpenController(int joystickID);
	static bool AddController(int joystickID);
	static int FindController(int joystickID);
	static bool RemoveController(int controllerID);
	static void SetLastState();
	static void RespondToEvent(AppEvent& event);
	static void Poll();
	static Uint16 CheckKeyModifiers(Uint16 modifiers);
	static bool IsKeyDown(int key);
	static bool IsKeyPressed(int key);
	static bool IsKeyReleased(int key);
	static void SetMouseMode(int mode);
	static Controller* GetController(int index);
	static bool ControllerIsConnected(int index);
	static bool ControllerIsXbox(int index);
	static bool ControllerIsPlayStation(int index);
	static bool ControllerIsJoyCon(int index);
	static bool ControllerHasShareButton(int index);
	static bool ControllerHasMicrophoneButton(int index);
	static bool ControllerHasPaddles(int index);
	static bool ControllerIsButtonHeld(int index, int button);
	static bool ControllerIsButtonPressed(int index, int button);
	static float ControllerGetAxis(int index, int axis);
	static int ControllerGetType(int index);
	static char* ControllerGetName(int index);
	static void ControllerSetPlayerIndex(int index, int player_index);
	static bool ControllerHasRumble(int index);
	static bool ControllerIsRumbleActive(int index);
	static bool
	ControllerRumble(int index, float large_frequency, float small_frequency, int duration);
	static bool ControllerRumble(int index, float strength, int duration);
	static void ControllerStopRumble(int index);
	static void ControllerStopRumble();
	static bool ControllerIsRumblePaused(int index);
	static void ControllerSetRumblePaused(int index, bool paused);
	static bool ControllerSetLargeMotorFrequency(int index, float frequency);
	static bool ControllerSetSmallMotorFrequency(int index, float frequency);
	static float TouchGetX(int touch_index);
	static float TouchGetY(int touch_index);
	static bool TouchIsDown(int touch_index);
	static bool TouchIsPressed(int touch_index);
	static bool TouchIsReleased(int touch_index);
	static int AddPlayer();
	static int GetPlayerCount();
	static void SetPlayerControllerIndex(unsigned playerID, int index);
	static int GetPlayerControllerIndex(unsigned playerID);
	static bool IsActionHeld(unsigned playerID, unsigned actionID);
	static bool IsActionPressed(unsigned playerID, unsigned actionID);
	static bool IsActionReleased(unsigned playerID, unsigned actionID);
	static bool IsAnyActionHeld(unsigned playerID);
	static bool IsAnyActionPressed(unsigned playerID);
	static bool IsAnyActionReleased(unsigned playerID);
	static bool IsActionHeld(unsigned playerID, unsigned actionID, unsigned device);
	static bool IsActionPressed(unsigned playerID, unsigned actionID, unsigned device);
	static bool IsActionReleased(unsigned playerID, unsigned actionID, unsigned device);
	static bool IsAnyActionHeld(unsigned playerID, unsigned device);
	static bool IsAnyActionPressed(unsigned playerID, unsigned device);
	static bool IsAnyActionReleased(unsigned playerID, unsigned device);
	static bool IsActionHeldByAny(unsigned actionID);
	static bool IsActionPressedByAny(unsigned actionID);
	static bool IsActionReleasedByAny(unsigned actionID);
	static bool IsActionHeldByAny(unsigned actionID, unsigned device);
	static bool IsActionPressedByAny(unsigned actionID, unsigned device);
	static bool IsActionReleasedByAny(unsigned actionID, unsigned device);
	static bool IsAnyActionHeldByAny();
	static bool IsAnyActionPressedByAny();
	static bool IsAnyActionReleasedByAny();
	static bool IsAnyActionHeldByAny(unsigned device);
	static bool IsAnyActionPressedByAny(unsigned device);
	static bool IsAnyActionReleasedByAny(unsigned device);
	static bool IsPlayerUsingDevice(unsigned playerID, unsigned device);
	static float GetAnalogActionInput(unsigned playerID, unsigned actionID);
	static InputBind*
	GetPlayerInputBind(unsigned playerID, unsigned actionID, unsigned index, bool isDefault);
	static bool SetPlayerInputBind(unsigned playerID,
		unsigned actionID,
		InputBind* bind,
		unsigned index,
		bool isDefault);
	static int
	AddPlayerInputBind(unsigned playerID, unsigned actionID, InputBind* bind, bool isDefault);
	static bool
	RemovePlayerInputBind(unsigned playerID, unsigned actionID, unsigned index, bool isDefault);
	static int GetPlayerInputBindCount(unsigned playerID, unsigned actionID, bool isDefault);
	static void ClearPlayerBinds(unsigned playerID, unsigned actionID, bool isDefault);
	static bool IsBindIndexValid(unsigned playerID, unsigned actionID, unsigned index);
	static void ResetPlayerBinds(unsigned playerID);
	static void ClearPlayers();
	static int RegisterAction(const char* name);
	static int GetActionID(const char* name);
	static void ClearInputs();
	static void InitPlayerControls();
	static void Dispose();
};

#endif /* ENGINE_INPUTMANAGER_H */
