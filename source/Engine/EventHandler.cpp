#include <Engine/Application.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Bytecode/VMThread.h>
#include <Engine/EventHandler.h>

#include <queue>

std::vector<EventHandler*> EventHandler::RegisteredHandlers[MAX_APPEVENT];
std::vector<EventHandler*> EventHandler::AllHandlers;

std::queue<AppEvent> Queue;

bool EventHandler::Handle(AppEvent& event) {
    if (!Enabled) {
        return false;
    }

    Obj* handler = (Obj*)Callback.Function;

    VMThread* thread = ScriptManager::Threads + 0;
    VMValue* stackTop = thread->StackTop;
    VMValue result;

    switch (event.Type) {
    case APPEVENT_QUIT:
        thread->Push(INTEGER_VAL(event.Quit.IsUserRequested));
        break;
    case APPEVENT_KEY_DOWN:
    case APPEVENT_KEY_UP:
        thread->Push(INTEGER_VAL(event.Keyboard.Key));
        break;
    case APPEVENT_WINDOW_RESIZE:
        thread->Push(INTEGER_VAL(event.Window.Width));
        thread->Push(INTEGER_VAL(event.Window.Height));
        thread->Push(INTEGER_VAL(event.Window.Index));
        break;
    case APPEVENT_WINDOW_MOVE:
        thread->Push(INTEGER_VAL(event.Window.X));
        thread->Push(INTEGER_VAL(event.Window.Y));
        thread->Push(INTEGER_VAL(event.Window.Index));
        break;
    case APPEVENT_WINDOW_DISPLAY_CHANGE:
        thread->Push(INTEGER_VAL(event.Window.X));
        thread->Push(INTEGER_VAL(event.Window.Index));
        break;
    case APPEVENT_WINDOW_MINIMIZE:
    case APPEVENT_WINDOW_MAXIMIZE:
    case APPEVENT_WINDOW_RESTORE:
    case APPEVENT_WINDOW_GAIN_INPUT_FOCUS:
    case APPEVENT_WINDOW_LOSE_INPUT_FOCUS:
    case APPEVENT_WINDOW_GAIN_MOUSE_FOCUS:
    case APPEVENT_WINDOW_LOSE_MOUSE_FOCUS:
        thread->Push(INTEGER_VAL(event.Window.Index));
        break;
    case APPEVENT_CONTROLLER_BUTTON_DOWN:
    case APPEVENT_CONTROLLER_BUTTON_UP:
        thread->Push(INTEGER_VAL(event.ControllerButton.Which));
        thread->Push(INTEGER_VAL(event.ControllerButton.Index));
        break;
    case APPEVENT_CONTROLLER_AXIS_MOTION:
        thread->Push(INTEGER_VAL(event.ControllerAxis.Which));
        thread->Push(DECIMAL_VAL(event.ControllerAxis.Value));
        thread->Push(INTEGER_VAL(event.ControllerAxis.Index));
        break;
    case APPEVENT_CONTROLLER_ADD:
    case APPEVENT_CONTROLLER_REMOVE:
        thread->Push(INTEGER_VAL(event.ControllerDevice.Index));
        break;
    case APPEVENT_MOUSE_BUTTON_DOWN:
    case APPEVENT_MOUSE_BUTTON_UP:
        thread->Push(INTEGER_VAL(event.Mouse.X));
        thread->Push(INTEGER_VAL(event.Mouse.Y));
        thread->Push(INTEGER_VAL(event.Mouse.Button));
        thread->Push(INTEGER_VAL(event.Mouse.Clicks));
        thread->Push(INTEGER_VAL(event.Mouse.WindowID));
        break;
    case APPEVENT_MOUSE_MOTION:
        thread->Push(INTEGER_VAL(event.Mouse.X));
        thread->Push(INTEGER_VAL(event.Mouse.Y));
        thread->Push(INTEGER_VAL(event.Mouse.MotionX));
        thread->Push(INTEGER_VAL(event.Mouse.MotionY));
        thread->Push(INTEGER_VAL(event.Mouse.WindowID));
        break;
    case APPEVENT_MOUSE_WHEEL_MOTION:
        thread->Push(INTEGER_VAL(event.MouseWheel.X));
        thread->Push(INTEGER_VAL(event.MouseWheel.Y));
        thread->Push(DECIMAL_VAL(event.MouseWheel.MotionX));
        thread->Push(DECIMAL_VAL(event.MouseWheel.MotionY));
        thread->Push(INTEGER_VAL(event.MouseWheel.WindowID));
        break;
    case APPEVENT_TOUCH_FINGER_MOTION:
        thread->Push(DECIMAL_VAL(event.Finger.X));
        thread->Push(DECIMAL_VAL(event.Finger.Y));
        thread->Push(DECIMAL_VAL(event.Finger.MotionX));
        thread->Push(DECIMAL_VAL(event.Finger.MotionY));
        thread->Push(DECIMAL_VAL(event.Finger.Pressure));
        thread->Push(INTEGER_VAL(event.Finger.Index));
        thread->Push(INTEGER_VAL(event.Finger.WindowID));
        break;
    case APPEVENT_TOUCH_FINGER_DOWN:
    case APPEVENT_TOUCH_FINGER_UP:
        thread->Push(DECIMAL_VAL(event.Finger.X));
        thread->Push(DECIMAL_VAL(event.Finger.Y));
        thread->Push(DECIMAL_VAL(event.Finger.Pressure));
        thread->Push(INTEGER_VAL(event.Finger.Index));
        thread->Push(INTEGER_VAL(event.Finger.WindowID));
        break;
    default:
        break;
    }

    VMValue callee = OBJECT_VAL(handler);

    // Check how many arguments we should actually pass.
    // For example, we can push 5 arguments, even though the function has a max arity of 2.
    // In that situation, we need to discard 3 arguments, as if we didn't pass them at all.
    int numArgs = thread->StackTop - stackTop;
    int minArity, maxArity;
    if (thread->GetArity(callee, minArity, maxArity) && numArgs > maxArity) {
        numArgs = maxArity;
        thread->StackTop = stackTop + numArgs;
    }

    result = thread->InvokeForEntity(callee, numArgs);
    thread->StackTop = stackTop;

    return Value::Truthy(result);
}
void EventHandler::Register(AppEventType type, EventHandlerCallback callback) {
    EventHandler* handler = new EventHandler;
    handler->Callback = callback;
    handler->Enabled = true;

    RegisteredHandlers[type].push_back(handler);
    AllHandlers.push_back(handler);
}
void EventHandler::RemoveAll() {
    for (size_t i = 0; i < MAX_APPEVENT; i++) {
        RegisteredHandlers[i].clear();
    }

    AllHandlers.clear();
}

void EventHandler::Push(AppEvent event) {
    Queue.push(event);
}
bool EventHandler::CallHandlers(AppEvent& event) {
    if (ScriptManager::ThreadCount == 0) {
        return false;
    }

    std::vector<EventHandler*>* list = &RegisteredHandlers[event.Type];
    int numHandlers = list->size();
    if (numHandlers == 0) {
        return false;
    }

    for (int i = numHandlers - 1; i >= 0; i--) {
        EventHandler* handler = (*list)[i];
        if (handler->Handle(event)) {
            return true;
        }
    }

    return false;
}

// Events that can be delayed a bit, and that the game may need to handle before the application does.
void EventHandler::Process(bool doCallbacks) {
    for (; !Queue.empty(); Queue.pop()) {
        AppEvent& event = Queue.front();

        bool handled = doCallbacks && CallHandlers(event);

        switch (event.Type) {
        case APPEVENT_QUIT:
            if (event.Quit.IsUserRequested && handled) {
                // If this was an user-requested quit (from pressing the 'X' button, for example)
                // then the callback can prevent the application from quitting.
                // Otherwise - for example, calling Application.Quit in a script - the application
                // always exits.
                break;
            }
            Application::Running = false;
            break;
        case APPEVENT_KEY_DOWN:
        case APPEVENT_KEY_UP:
        case APPEVENT_MOUSE_MOTION:
        case APPEVENT_MOUSE_BUTTON_DOWN:
        case APPEVENT_MOUSE_BUTTON_UP:
        case APPEVENT_MOUSE_WHEEL_MOTION:
        case APPEVENT_CONTROLLER_BUTTON_DOWN:
        case APPEVENT_CONTROLLER_BUTTON_UP:
        case APPEVENT_CONTROLLER_AXIS_MOTION:
        case APPEVENT_TOUCH_FINGER_MOTION:
        case APPEVENT_TOUCH_FINGER_DOWN:
        case APPEVENT_TOUCH_FINGER_UP:
            if (handled) {
                break;
            }

            if (event.Type == APPEVENT_KEY_DOWN || event.Type == APPEVENT_KEY_UP) {
                Application::HandleBinds(event);
            }

            InputManager::RespondToEvent(event);
            break;
        default:
            break;
        }
    }
}
