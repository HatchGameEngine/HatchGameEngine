#ifndef ENGINE_INPUT_INPUTPLAYER_H
#define ENGINE_INPUT_INPUTPLAYER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Input/Input.h>
#include <Engine/Input/InputAction.h>

class InputPlayer {
private:
	size_t PushBindToList(PlayerInputConfig& config, InputBind* def);
	bool ReplaceBindInList(PlayerInputConfig& config, InputBind* def, unsigned index);
	bool RemoveBindFromList(PlayerInputConfig& config, unsigned index);
	InputBind* GetBindAtIndex(PlayerInputConfig& config, unsigned index);
	size_t GetBindCount(PlayerInputConfig& config);
	void CopyDefaultBinds(InputPlayer& src, bool filter, int filterType);
	void ResetBindList(PlayerInputConfig& dest, PlayerInputConfig& src);
	bool CheckInputBindState(InputBind* bind, bool held);
	bool CheckIfInputHeld(unsigned actionID, unsigned device);
	bool CheckIfInputPressed(unsigned actionID, unsigned device);
	bool IsControllerBindHeld(unsigned num);
	bool IsControllerBindPressed(unsigned num);
	void UpdateControllerBind(unsigned num);
	bool GetAnalogControllerBind(unsigned num, float& result);

public:
	int ID;
	vector<PlayerInputConfig> Binds;
	vector<PlayerInputConfig> DefaultBinds;
	PlayerInputStatus Status[InputDevice_MAX];
	PlayerInputStatus AllStatus;
	vector<Uint8> NumHeld;
	vector<Uint8> NumPressed;
	vector<Uint8> NumReleased;
	bool AnyHeld;
	bool AnyPressed;
	bool AnyReleased;
	vector<Uint8> ControllerState;
	bool IsUsingDevice[InputDevice_MAX];
	int ControllerIndex = -1;
	const char* ConfigFilename = nullptr;

	InputPlayer(int id);
	void SetNumActions(size_t num);
	void ClearBinds();
	void ResetBinds();
	void Update();
	int AddBind(unsigned num, InputBind* bind);
	bool ReplaceBind(unsigned num, InputBind* bind, unsigned index);
	bool RemoveBind(unsigned num, unsigned index);
	InputBind* GetBind(unsigned num, unsigned index);
	size_t GetBindCount(unsigned num);
	int AddDefaultBind(unsigned num, InputBind* bind);
	void CopyDefaultBinds(InputPlayer& src);
	void CopyDefaultBinds(InputPlayer& src, int filterType);
	bool ReplaceDefaultBind(unsigned num, InputBind* bind, unsigned index);
	bool RemoveDefaultBind(unsigned num, unsigned index);
	InputBind* GetDefaultBind(unsigned num, unsigned index);
	size_t GetDefaultBindCount(unsigned num);
	bool IsBindIndexValid(unsigned num, unsigned index);
	void ClearBinds(unsigned num);
	void ClearDefaultBinds(unsigned num);
	void ResetBind(unsigned num);
	bool IsInputHeld(unsigned actionID, unsigned device);
	bool IsInputPressed(unsigned actionID, unsigned device);
	bool IsInputReleased(unsigned actionID, unsigned device);
	bool IsAnyInputHeld(unsigned device);
	bool IsAnyInputPressed(unsigned device);
	bool IsAnyInputReleased(unsigned device);
	bool IsInputHeld(unsigned actionID);
	bool IsInputPressed(unsigned actionID);
	bool IsInputReleased(unsigned actionID);
	bool IsAnyInputHeld();
	bool IsAnyInputPressed();
	bool IsAnyInputReleased();
	float GetAnalogActionInput(unsigned actionID);
};

#endif /* ENGINE_INPUT_INPUTPLAYER_H */
