#include <Engine/Input/InputAction.h>

InputAction::InputAction(const char* name, unsigned id) {
	Name = std::string(name);
	ID = id;
}
InputAction::~InputAction() {}
