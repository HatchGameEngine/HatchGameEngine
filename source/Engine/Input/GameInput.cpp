#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Input/Input.h>

class GameInput {
public:
    unsigned ID;
    string   Name;
};
#endif

#include <Engine/Input/GameInput.h>

PUBLIC GameInput::GameInput(const char* name, unsigned id) {
    Name = std::string(name);
    ID = id;
}
PUBLIC GameInput::~GameInput() {

}
