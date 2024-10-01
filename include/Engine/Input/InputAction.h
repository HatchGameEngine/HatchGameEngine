#ifndef ENGINE_INPUT_INPUTACTION_H
#define ENGINE_INPUT_INPUTACTION_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


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
