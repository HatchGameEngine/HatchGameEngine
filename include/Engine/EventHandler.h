#ifndef ENGINE_EVENTHANDLER_H
#define ENGINE_EVENTHANDLER_H

#include <Engine/Includes/AppEvent.h>

struct EventHandlerCallback {
	void* Function;
};

class EventHandler {
private:
	AppEventType Type;
	unsigned ID;
	bool Enabled;

	static EventHandler* FindByID(unsigned id);
	static bool CallHandlers(AppEvent& event);

public:
	EventHandlerCallback Callback;

	bool Handle(AppEvent& event);

	static std::vector<EventHandler*> List;

	static int Register(AppEventType type, EventHandlerCallback callback);
	static bool IsValid(unsigned id);
	static bool SetEnabled(unsigned id, bool enabled);
	static bool Remove(unsigned id);
	static void RemoveAll();

	static void Push(AppEvent event);
	static void Process(bool doCallbacks);
};

#endif /* ENGINE_EVENTHANDLER_H */
