#ifndef ENGINE_INPUTMANAGER_H
#define ENGINE_INPUTMANAGER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Input/Controller.h>
#include <Engine/Application.h>

class InputManager {
private:
    static int FindController(int joystickID);

public:
    static float               MouseX;
    static float               MouseY;
    static int                 MouseDown;
    static int                 MousePressed;
    static int                 MouseReleased;
    static Uint8               KeyboardState[0x120];
    static Uint8               KeyboardStateLast[0x120];
    static int                 NumControllers;
    static vector<Controller*> Controllers;
    static SDL_TouchID         TouchDevice;
    static void*               TouchStates;

    static void  Init();
    static Controller* OpenController(int index);
    static void  InitControllers();
    static bool  AddController(int index);
    static void  RemoveController(int joystickID);
    static void  Poll();
    static bool  ControllerIsConnected(int index);
    static bool  ControllerGetButton(int index, int button);
    static int   ControllerGetHat(int index, int hat);
    static float ControllerGetAxis(int index, int axis);
    static int   ControllerGetType(int index);
    static char* ControllerGetName(int index);
    static void  ControllerSetPlayerIndex(int index, int player_index);
    static bool  ControllerHasRumble(int index);
    static bool  ControllerIsRumbleActive(int index);
    static bool  ControllerRumble(int index, float large_frequency, float small_frequency, int duration);
    static bool  ControllerRumble(int index, float strength, int duration);
    static void  ControllerStopRumble(int index);
    static bool  ControllerGetRumblePaused(int index);
    static void  ControllerSetRumblePaused(int index, bool paused);
    static bool  ControllerSetLargeMotorFrequency(int index, float frequency);
    static bool  ControllerSetSmallMotorFrequency(int index, float frequency);
    static float TouchGetX(int touch_index);
    static float TouchGetY(int touch_index);
    static bool  TouchIsDown(int touch_index);
    static bool  TouchIsPressed(int touch_index);
    static bool  TouchIsReleased(int touch_index);
    static void  Dispose();
};

#endif /* ENGINE_INPUTMANAGER_H */
