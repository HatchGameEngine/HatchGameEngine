#ifndef ENGINE_DEVELOPERMENU_H
#define ENGINE_DEVELOPERMENU_H

#include <Engine/Includes/Standard.h>

enum {
	SCENEBROWSER_DEFAULT,
	SCENEBROWSER_RESOURCES,
	SCENEBROWSER_SCENELIST
};

struct DeveloperMenu {
	void (*State)();
	int Selection;
	int SubSelection;
	int ScrollPos;
	int SubScrollPos;
	double Timer;
	bool Fullscreen;
	int SceneState;
	int ListPos;
	int WindowScale;
	bool WindowBorderless;
	int CurrentWindowWidth;
	int CurrentWindowHeight;
	int PlayerListPos;
	bool MusicPausedStore;
	bool ResourcesBrowserAvailable;
	int SceneBrowserMode;
};

#endif /* ENGINE_DEVELOPERMENU_H */
