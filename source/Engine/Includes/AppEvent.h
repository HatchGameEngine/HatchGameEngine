#ifndef APPEVENT_H
#define APPEVENT_H

#include <Engine/Includes/Standard.h>

enum AppEventType {
    APPEVENT_QUIT,

    APPEVENT_WINDOW_MOVE,
    APPEVENT_WINDOW_RESIZE,
    APPEVENT_WINDOW_MINIMIZE,
    APPEVENT_WINDOW_MAXIMIZE,
    APPEVENT_WINDOW_RESTORE,
    APPEVENT_WINDOW_GAIN_INPUT_FOCUS,
    APPEVENT_WINDOW_LOSE_INPUT_FOCUS,
    APPEVENT_WINDOW_GAIN_MOUSE_FOCUS,
    APPEVENT_WINDOW_LOSE_MOUSE_FOCUS,
    APPEVENT_WINDOW_DISPLAY_CHANGE,

    APPEVENT_KEY_DOWN,
    APPEVENT_KEY_UP,

    APPEVENT_MOUSE_MOTION,
    APPEVENT_MOUSE_BUTTON_DOWN,
    APPEVENT_MOUSE_BUTTON_UP,
    APPEVENT_MOUSE_WHEEL_MOTION,

    APPEVENT_CONTROLLER_BUTTON_DOWN,
    APPEVENT_CONTROLLER_BUTTON_UP,
    APPEVENT_CONTROLLER_AXIS_MOTION,
    APPEVENT_CONTROLLER_ADD,
    APPEVENT_CONTROLLER_REMOVE,

    APPEVENT_FINGER_MOTION,
    APPEVENT_FINGER_DOWN,
    APPEVENT_FINGER_UP,

    APPEVENT_AUDIO_DEVICE_ADD,
    APPEVENT_AUDIO_DEVICE_REMOVE,

    MAX_APPEVENT
};

struct AppEvent {
    AppEventType Type;

    union {
        struct {
            int X;
            int Y;
            int Width;
            int Height;
            Uint8 Index;
        } Window;

        struct {
            Uint16 Key;
        } Keyboard;

        struct {
            Uint8 Button;
            Uint8 Clicks;
            Sint32 X;
            Sint32 Y;
            Sint32 MotionX;
            Sint32 MotionY;
            Uint8 WindowID;
        } Mouse;

        struct {
            float MotionX;
            float MotionY;
            Sint32 X;
            Sint32 Y;
            Uint8 WindowID;
        } MouseWheel;

        struct {
            Uint8 Which;
            int Index;
        } ControllerButton;

        struct {
            Uint8 Which;
            float Value;
            int Index;
        } ControllerAxis;

        struct {
            int Index;
        } ControllerDevice;
    };
};

#define KEY_APPEVENT(key, type) \
    AppEvent event; \
    event.Type = type; \
    event.Keyboard.Key = key

#define MOUSE_APPEVENT(e, type) \
    AppEvent event; \
    event.Type = type; \
    event.Mouse.Button = e.button.button - 1; \
    event.Mouse.X = e.button.x; \
    event.Mouse.Y = e.button.y; \
    event.Mouse.Clicks = e.button.clicks; \
    event.Mouse.WindowID = e.button.windowID

#define MOUSE_MOTION_APPEVENT(e) \
    AppEvent event; \
    event.Type = APPEVENT_MOUSE_MOTION; \
    event.Mouse.X = e.motion.x; \
    event.Mouse.Y = e.motion.y; \
    event.Mouse.MotionX = e.motion.xrel; \
    event.Mouse.MotionY = e.motion.yrel; \
    event.Mouse.WindowID = e.motion.windowID

#define MOUSE_WHEEL_APPEVENT(e) \
    AppEvent event; \
    event.Type = APPEVENT_MOUSE_WHEEL_MOTION; \
    event.MouseWheel.MotionX = e.wheel.preciseX; \
    event.MouseWheel.MotionY = e.wheel.preciseY; \
    event.MouseWheel.X = e.wheel.mouseX; \
    event.MouseWheel.Y = e.wheel.mouseY; \
    event.MouseWheel.WindowID = e.wheel.windowID

#define CONTROLLER_BUTTON_APPEVENT(index, button, type) \
    AppEvent event; \
    event.Type = type; \
    event.ControllerButton.Index = index; \
    event.ControllerButton.Which = button

#define CONTROLLER_AXIS_APPEVENT(index, axis, value) \
    AppEvent event; \
    event.Type = APPEVENT_CONTROLLER_AXIS_MOTION; \
    event.ControllerAxis.Index = index; \
    event.ControllerAxis.Which = axis; \
    event.ControllerAxis.Value = value

#define CONTROLLER_DEVICE_APPEVENT(index, type) \
    AppEvent event; \
    event.Type = type; \
    event.ControllerDevice.Index = index

#define WINDOW_APPEVENT(e, type) \
    AppEvent event; \
    event.Type = type; \
    event.Window.Index = e.window.windowID

#endif
