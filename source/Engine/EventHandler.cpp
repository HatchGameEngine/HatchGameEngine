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
    case APPEVENT_KEY_DOWN:
    case APPEVENT_KEY_UP:
        thread->Push(INTEGER_VAL(event.Keyboard.Key));
        break;
    case APPEVENT_WINDOW_RESIZE:
        thread->Push(INTEGER_VAL(event.Window.Width));
        thread->Push(INTEGER_VAL(event.Window.Height));
        break;
    case APPEVENT_CONTROLLER_ADD:
    case APPEVENT_CONTROLLER_REMOVE:
        thread->Push(INTEGER_VAL(event.Controller.Index));
        break;
    default:
        break;
    }

    result = thread->InvokeForEntity(OBJECT_VAL(handler), thread->StackTop - stackTop);
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

// Events that can be delayed a bit, and that the game may need to handle.
void EventHandler::Process() {
    for (; !Queue.empty(); Queue.pop()) {
        AppEvent& event = Queue.front();

        bool handled = CallHandlers(event);

        switch (event.Type) {
        case APPEVENT_KEY_DOWN:
            if (handled) {
                break;
            }
            Application::HandleBinds(event.Keyboard.Key);
            break;
        default:
            break;
        }
    }
}
