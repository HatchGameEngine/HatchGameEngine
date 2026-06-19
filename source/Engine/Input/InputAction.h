#ifndef ENGINE_INPUT_INPUTACTION_H
#define ENGINE_INPUT_INPUTACTION_H

#include <Engine/Includes/Standard.h>
#include <Engine/Input/Input.h>

class InputAction {
public:
	unsigned ID;
	string Name;

	InputAction(const char* name, unsigned id);
	~InputAction();
};

#endif /* ENGINE_INPUT_INPUTACTION_H */
