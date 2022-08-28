#ifndef ENGINE_INPUT_CONTROLLER_H
#define ENGINE_INPUT_CONTROLLER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Input/Input.h>
#include <Engine/Application.h>

class Controller {
private:
    static ControllerType DetermineType(void* gamecontroller);

public:
    ControllerType      Type;
    bool                Connected;
    ControllerRumble*   Rumble;
    SDL_GameController* Device;
    SDL_Joystick*       JoystickDevice;
    SDL_JoystickID      JoystickID;

    Controller(int index);
    ~Controller();
    bool          Open(int index);
    void          Close();
    void          Reset();
    char*          GetName();
    void           SetPlayerIndex(int index);
    bool          GetButton(int button);
    float          GetAxis(int axis);
    void           Update();
    bool          IsXbox();
    bool          IsPlaystation();
    bool          IsJoyCon();
    bool          HasShareButton();
    bool          HasPaddles();
    bool          HasMicrophoneButton();
};

#endif /* ENGINE_INPUT_CONTROLLER_H */
