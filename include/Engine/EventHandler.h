#ifndef ENGINE_EVENTHANDLER_H
#define ENGINE_EVENTHANDLER_H

#include <Engine/Includes/AppEvent.h>

struct EventHandlerCallback {
	void* Function;
};

class EventHandler {
private:
	bool Enabled;

	static bool CallHandlers(AppEvent& event);

public:
	EventHandlerCallback Callback;

	bool Handle(AppEvent& event);

	static std::vector<EventHandler*> RegisteredHandlers[MAX_APPEVENT];
	static std::vector<EventHandler*> AllHandlers;

	static void Register(AppEventType type, EventHandlerCallback callback);
	static void RemoveAll();

	static void Push(AppEvent event);
	static void Process(bool doCallbacks);
};

#endif /* ENGINE_EVENTHANDLER_H */
