#ifndef ENGINE_EVENTHANDLER_H
#define ENGINE_EVENTHANDLER_H

#include <Engine/Includes/AppEvent.h>

struct EventHandlerCallback {
	void* Function;
};

class EventHandler {
private:
	AppEventType Type;
	bool Enabled;

	static bool CallHandlers(AppEvent& event);

public:
	EventHandlerCallback Callback;

	bool Handle(AppEvent& event);

	static std::vector<EventHandler*> List;
	static std::vector<EventHandler*> GroupedByType[MAX_APPEVENT];

	static int Register(AppEventType type, EventHandlerCallback callback);
	static bool IsValidIndex(int index);
	static void SetEnabled(int index, bool enabled);
	static void Remove(int index);
	static void RemoveAll();

	static void Push(AppEvent event);
	static void Process(bool doCallbacks);
};

#endif /* ENGINE_EVENTHANDLER_H */
