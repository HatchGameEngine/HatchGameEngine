#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Input/Input.h>

class InputAction {
public:
    unsigned ID;
    string   Name;
};
#endif

#include <Engine/Input/InputAction.h>

PUBLIC InputAction::InputAction(const char* name, unsigned id) {
    Name = std::string(name);
    ID = id;
}
PUBLIC InputAction::~InputAction() {

}
