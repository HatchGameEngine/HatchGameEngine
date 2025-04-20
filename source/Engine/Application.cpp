#include <Engine/Application.h>
#include <Engine/Graphics.h>

#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Diagnostics/MemoryPools.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/VFS/MemoryCache.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/Scene/SceneInfo.h>
#include <Engine/TextFormats/XML/XMLNode.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/Utilities/StringUtils.h>

#include <Engine/Media/MediaPlayer.h>
#include <Engine/Media/MediaSource.h>

#ifdef IOS
extern "C" {
#include <Engine/Platforms/iOS/MediaPlayer.h>
}
#endif

#ifdef MACOSX
#include <Engine/Platforms/MacOS/Filesystem.h>
#include <unistd.h>
#endif

#ifdef MSYS
#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#endif

#define DEFAULT_SETTINGS_FILENAME "config://config.ini"
#define DEFAULT_MAX_FRAMESKIP 15

#if WIN32
Platforms Application::Platform = Platforms::Windows;
#elif MACOSX
Platforms Application::Platform = Platforms::MacOS;
#elif LINUX
Platforms Application::Platform = Platforms::Linux;
#elif SWITCH
Platforms Application::Platform = Platforms::Switch;
#elif PLAYSTATION
Platforms Application::Platform = Platforms::PlayStation;
#elif XBOX
Platforms Application::Platform = Platforms::Xbox;
#elif ANDROID
Platforms Application::Platform = Platforms::Android;
#elif IOS
Platforms Application::Platform = Platforms::iOS;
#else
Platforms Application::Platform = Platforms::Unknown;
#endif

vector<char*> Application::CmdLineArgs;

INI* Application::Settings = NULL;
char Application::SettingsFile[MAX_PATH_LENGTH];

XMLNode* Application::GameConfig = NULL;

int Application::TargetFPS = DEFAULT_TARGET_FRAMERATE;
float Application::CurrentFPS = DEFAULT_TARGET_FRAMERATE;
bool Application::Running = false;
bool Application::FirstFrame = true;
bool Application::GameStart = false;
bool Application::PortableMode = false;

SDL_Window* Application::Window = NULL;
char Application::WindowTitle[256];
int Application::WindowWidth = 848;
int Application::WindowHeight = 480;
int Application::DefaultMonitor = 0;

char Application::EngineVersion[256];

char Application::GameTitle[256];
char Application::GameTitleShort[256];
char Application::GameVersion[256];
char Application::GameDescription[256];

int Application::UpdatesPerFrame = 1;
int Application::FrameSkip = DEFAULT_MAX_FRAMESKIP;
bool Application::Stepper = false;
bool Application::Step = false;

int Application::MasterVolume = 100;
int Application::MusicVolume = 100;
int Application::SoundVolume = 100;

bool Application::DevMenuActivated = false;
bool Application::DevConvertModels = false;

bool Application::AllowCmdLineSceneLoad = false;

char StartingScene[256];
char LogFilename[MAX_PATH_LENGTH];

bool UseMemoryFileCache = false;

bool DevMenu = false;
bool ShowFPS = false;
bool TakeSnapshot = false;
bool DoNothing = false;
int UpdatesPerFastForward = 4;

int BenchmarkFrameCount = 0;
double BenchmarkTickStart = 0.0;

double Overdelay = 0.0;
double FrameTimeStart = 0.0;
double FrameTimeDesired = 1000.0 / Application::TargetFPS;

int KeyBinds[(int)KeyBind::Max];

ISprite* DEBUG_fontSprite = NULL;
void DEBUG_DrawText(char* text, float x, float y) {
	for (char* i = text; *i; i++) {
		Graphics::DrawSprite(
			DEBUG_fontSprite, 0, (int)*i, x, y, false, false, 1.0f, 1.0f, 0.0f);
		x += 14; // DEBUG_fontSprite->Animations[0].Frames[(int)*i].ID;
	}
}

void Application::Init(int argc, char* args[]) {
#ifdef MSYS
	AllocConsole();
	freopen_s((FILE**)stdin, "CONIN$", "w", stdin);
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
#endif

	Application::MakeEngineVersion();

#ifdef MACOSX
	// Set environment
	char appSupportPath[MAX_PATH_LENGTH];
	int isBundle = MacOS_GetApplicationSupportDirectory(appSupportPath, sizeof appSupportPath);
	if (isBundle) {
		strcat(appSupportPath, "/" TARGET_NAME);
		if (!Directory::Exists(appSupportPath)) {
			Directory::Create(appSupportPath);
		}
		chdir(appSupportPath);
	}
#endif

#ifdef PORTABLE_MODE
	PortableMode = true;
#endif

	Log::Init();

	MemoryPools::Init();

	SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");
	SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");

#ifdef IOS
	// SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft
	// LandscapeRight Portrait PortraitUpsideDown"); // iOS only
	// SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback"); //
	// Background Playback
	SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "1");
	iOS_InitMediaPlayer();
#endif

#ifdef ANDROID
	SDL_SetHint(SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH, "1");
#endif

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK |
		    SDL_INIT_GAMECONTROLLER) < 0) {
		Log::Print(Log::LOG_INFO, "SDL_Init failed with error: %s", SDL_GetError());
	}

	SDL_SetEventFilter(Application::HandleAppEvents, NULL);

	Application::SetTargetFrameRate(DEFAULT_TARGET_FRAMERATE);

	for (int i = 1; i < argc; i++) {
		Application::CmdLineArgs.push_back(StringUtils::Duplicate(args[i]));
	}

	// Initialize a few needed subsystems
	InputManager::Init();
	Clock::Init();

	// Load game stuff.
#ifdef ALLOW_COMMAND_LINE_RESOURCE_LOAD
	if (argc > 1 && !!StringUtils::StrCaseStr(args[1], ".hatch")) {
		ResourceManager::Init(args[1]);
	}
	else
#endif
		ResourceManager::Init(NULL);

	Application::LoadGameConfig();
	Application::ReloadSettings();

	// Open the log file immediately after
	Log::OpenFile(LogFilename);

	Application::LogEngineVersion();
	Application::LogSystemInfo();

	// Keep loading game stuff.
	Application::LoadGameInfo();
	Application::LoadSceneInfo();
	Application::InitPlayerControls();
	Application::DisposeGameConfig();

	// Needs to be done after the settings are read and before Graphics::Init()
	Graphics::ChooseBackend();

	// Note: The window is created hidden.
	Application::CreateWindow();

	// Continue initializing subsystems
	Math::Init();
	Graphics::Init();

	if (UseMemoryFileCache) {
		MemoryCache::Init();
	}

	AudioManager::Init();

	Running = true;
}
void Application::LogEngineVersion() {
#ifdef GIT_COMMIT_HASH
	Log::Print(Log::LOG_INFO,
		"Hatch Game Engine %s (commit " GIT_COMMIT_HASH ")",
		Application::EngineVersion);
#else
	Log::Print(Log::LOG_INFO, "Hatch Game Engine %s", Application::EngineVersion);
#endif
}
void Application::LogSystemInfo() {
	const char* platform;
	switch (Application::Platform) {
	case Platforms::Windows:
		platform = "Windows";
		break;
	case Platforms::MacOS:
		platform = "MacOS";
		break;
	case Platforms::Linux:
		platform = "Linux";
		break;
	case Platforms::Switch:
		platform = "Nintendo Switch";
		break;
	case Platforms::PlayStation:
		platform = "PlayStation";
		break;
	case Platforms::Xbox:
		platform = "Xbox";
		break;
	case Platforms::Android:
		platform = "Android";
		break;
	case Platforms::iOS:
		platform = "iOS";
		break;
	default:
		platform = "Unknown";
		break;
	}
	Log::Print(Log::LOG_INFO, "Current Platform: %s", platform);
	Log::Print(Log::LOG_VERBOSE, "CPU Core Count: %d", SDL_GetCPUCount());
	Log::Print(Log::LOG_INFO, "System Memory: %d MB", SDL_GetSystemRAM());
}
void Application::CreateWindow() {
	bool allowRetina = false;
	Application::Settings->GetBool("display", "retina", &allowRetina);

	int defaultMonitor = Application::DefaultMonitor;

	Uint32 window_flags = 0;
	window_flags |= IsPC() ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
	window_flags |= Graphics::GetWindowFlags();
	if (allowRetina) {
		window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
	}

	Application::Window = SDL_CreateWindow(NULL,
		SDL_WINDOWPOS_CENTERED_DISPLAY(defaultMonitor),
		SDL_WINDOWPOS_CENTERED_DISPLAY(defaultMonitor),
		Application::WindowWidth,
		Application::WindowHeight,
		window_flags);

	if (Application::Platform == Platforms::iOS) {
		SDL_SetWindowFullscreen(Application::Window, SDL_WINDOW_FULLSCREEN);
	}
	else if (Application::Platform == Platforms::Switch) {
		SDL_SetWindowFullscreen(Application::Window, SDL_WINDOW_FULLSCREEN);
		AudioManager::MasterVolume = 0.25;

#ifdef SWITCH
		SDL_DisplayMode mode;
		SDL_GetDisplayMode(0, 1 - appletGetOperationMode(), &mode);
		Log::Print(Log::LOG_INFO, "Display Mode: %i x %i", mode.w, mode.h);
#endif
	}
	else {
		bool fullscreen = false;
		Application::Settings->GetBool("display", "fullscreen", &fullscreen);

		if (Application::GetWindowFullscreen() != fullscreen) {
			Application::SetWindowFullscreen(fullscreen);
		}
	}

	Application::SetWindowTitle(Application::GameTitleShort);
}

void Application::SetTargetFrameRate(int targetFPS) {
	if (targetFPS < 1) {
		TargetFPS = 1;
	}
	else if (targetFPS > MAX_TARGET_FRAMERATE) {
		TargetFPS = MAX_TARGET_FRAMERATE;
	}
	else {
		TargetFPS = targetFPS;
	}

	FrameTimeDesired = 1000.0 / TargetFPS;
}

void Application::MakeEngineVersion() {
	std::string versionText =
		std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR);

#ifdef VERSION_PATCH
	versionText += ".";
	versionText += std::to_string(VERSION_PATCH);
#endif

#ifdef VERSION_PRERELEASE
	versionText += "-";
	versionText += VERSION_PRERELEASE;
#endif

#ifdef VERSION_CODENAME
	versionText += " (";
	versionText += VERSION_CODENAME;
	versionText += ")";
#endif

	if (versionText.size() > 0) {
		StringUtils::Copy(Application::EngineVersion,
			versionText.c_str(),
			sizeof(Application::EngineVersion));
	}
}

bool Application::IsPC() {
	return Application::Platform == Platforms::Windows ||
		Application::Platform == Platforms::MacOS ||
		Application::Platform == Platforms::Linux;
}
bool Application::IsMobile() {
	return Application::Platform == Platforms::iOS ||
		Application::Platform == Platforms::Android;
}

// Returns a "safe" version of the developer's name (for e.g. file names)
const char* Application::GetDeveloperIdentifier() {
	// TODO: Implement!
	return NULL;
}

// Returns a "safe" version of the game's name (for e.g. file names)
const char* Application::GetGameIdentifier() {
	// TODO: Implement!
	return "hatch";
}

// Returns the name of the saves directory
const char* Application::GetSavesDir() {
	// TODO: Implement!
	return "saves";
}

// Returns the name of the preferences directory
const char* Application::GetPreferencesDir() {
	// TODO: Implement!
	return NULL;
}

bool AutomaticPerformanceSnapshots = false;
double AutomaticPerformanceSnapshotFrameTimeThreshold = 20.0;
double AutomaticPerformanceSnapshotLastTime = 0.0;
double AutomaticPerformanceSnapshotMinInterval = 5000.0; // 5 seconds

int MetricFrameCounterTime = 0;
double MetricEventTime = -1;
double MetricAfterSceneTime = -1;
double MetricPollTime = -1;
double MetricUpdateTime = -1;
double MetricClearTime = -1;
double MetricRenderTime = -1;
double MetricFPSCounterTime = -1;
double MetricPresentTime = -1;
double MetricFrameTime = 0.0;
vector<ObjectList*> ListList;
void Application::GetPerformanceSnapshot() {
	if (Scene::ObjectLists) {
		// General Performance Snapshot
		double types[] = {MetricEventTime,
			MetricAfterSceneTime,
			MetricPollTime,
			MetricUpdateTime,
			MetricClearTime,
			MetricRenderTime,
			MetricPresentTime,
			0.0,
			MetricFrameTime,
			CurrentFPS};
		const char* typeNames[] = {
			"Event Polling:         %8.3f ms",
			"Garbage Collector:     %8.3f ms",
			"Input Polling:         %8.3f ms",
			"Entity Update:         %8.3f ms",
			"Clear Time:            %8.3f ms",
			"World Render Commands: %8.3f ms",
			"Frame Present Time:    %8.3f ms",
			"==================================",
			"Frame Total Time:      %8.3f ms",
			"FPS:                   %11.3f",
		};

		ListList.clear();
		Scene::ObjectLists->WithAll([](Uint32, ObjectList* list) -> void {
			if ((list->Performance.Update.AverageTime > 0.0 &&
				    list->Performance.Update.AverageItemCount > 0) ||
				(list->Performance.Render.AverageTime > 0.0 &&
					list->Performance.Render.AverageItemCount > 0)) {
				ListList.push_back(list);
			}
		});
		std::sort(
			ListList.begin(), ListList.end(), [](ObjectList* a, ObjectList* b) -> bool {
				ObjectListPerformanceStats& updatePerfA = a->Performance.Update;
				ObjectListPerformanceStats& updatePerfB = b->Performance.Update;
				ObjectListPerformanceStats& renderPerfA = a->Performance.Render;
				ObjectListPerformanceStats& renderPerfB = b->Performance.Render;
				return updatePerfA.AverageTime * updatePerfA.AverageItemCount +
					renderPerfA.AverageTime * renderPerfA.AverageItemCount >
					updatePerfB.AverageTime * updatePerfB.AverageItemCount +
					renderPerfB.AverageTime * renderPerfB.AverageItemCount;
			});

		Log::Print(Log::LOG_IMPORTANT, "General Performance Snapshot:");
		for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
			Log::Print(Log::LOG_INFO, typeNames[i], types[i]);
		}

		// View Rendering Performance Snapshot
		char layerText[2048];
		Log::Print(Log::LOG_IMPORTANT, "View Rendering Performance Snapshot:");
		for (int i = 0; i < MAX_SCENE_VIEWS; i++) {
			View* currentView = &Scene::Views[i];
			if (currentView->Active) {
				layerText[0] = 0;
				double tilesTotal = 0.0;
				for (size_t li = 0; li < Scene::Layers.size(); li++) {
					SceneLayer* layer = &Scene::Layers[li];
					char temp[128];
					snprintf(temp,
						sizeof(temp),
						"     > %24s:   %8.3f ms\n",
						layer->Name,
						Scene::PERF_ViewRender[i].LayerTileRenderTime[li]);
					StringUtils::Concat(layerText, temp, sizeof(layerText));
					tilesTotal +=
						Scene::PERF_ViewRender[i].LayerTileRenderTime[li];
				}
				Log::Print(Log::LOG_INFO,
					"View %d:\n"
					"           - Render Setup:        %8.3f ms %s\n"
					"           - Projection Setup:    %8.3f ms\n"
					"           - Object RenderEarly:  %8.3f ms\n"
					"           - Object Render:       %8.3f ms\n"
					"           - Object RenderLate:   %8.3f ms\n"
					"           - Layer Tiles Total:   %8.3f ms\n%s"
					"           - Finish:              %8.3f ms\n"
					"           - Total:               %8.3f ms",
					i,
					Scene::PERF_ViewRender[i].RenderSetupTime,
					Scene::PERF_ViewRender[i].RecreatedDrawTarget
						? "(recreated draw target)"
						: "",
					Scene::PERF_ViewRender[i].ProjectionSetupTime,
					Scene::PERF_ViewRender[i].ObjectRenderEarlyTime,
					Scene::PERF_ViewRender[i].ObjectRenderTime,
					Scene::PERF_ViewRender[i].ObjectRenderLateTime,
					tilesTotal,
					layerText,
					Scene::PERF_ViewRender[i].RenderFinishTime,
					Scene::PERF_ViewRender[i].RenderTime);
			}
		}

		// Object Performance Snapshot
		double totalUpdateEarly = 0.0;
		double totalUpdate = 0.0;
		double totalUpdateLate = 0.0;
		double totalRender = 0.0;
		Log::Print(Log::LOG_IMPORTANT, "Object Performance Snapshot:");
		for (size_t i = 0; i < ListList.size(); i++) {
			ObjectList* list = ListList[i];
			ObjectListPerformance& perf = list->Performance;
			Log::Print(Log::LOG_INFO,
				"Object \"%s\":\n"
				"           - Avg Update Early %6.1f mcs (Total %6.1f mcs, Count %d)\n"
				"           - Avg Update       %6.1f mcs (Total %6.1f mcs, Count %d)\n"
				"           - Avg Update Late  %6.1f mcs (Total %6.1f mcs, Count %d)\n"
				"           - Avg Render       %6.1f mcs (Total %6.1f mcs, Count %d)",
				list->ObjectName,
				perf.EarlyUpdate.GetAverageTime(),
				perf.EarlyUpdate.GetTotalAverageTime(),
				(int)perf.EarlyUpdate.AverageItemCount,
				perf.Update.GetAverageTime(),
				perf.Update.GetTotalAverageTime(),
				(int)perf.Update.AverageItemCount,
				perf.LateUpdate.GetAverageTime(),
				perf.LateUpdate.GetTotalAverageTime(),
				(int)perf.LateUpdate.AverageItemCount,
				perf.Render.GetAverageTime(),
				perf.Render.GetTotalAverageTime(),
				(int)perf.Render.AverageItemCount);

			totalUpdateEarly += perf.EarlyUpdate.GetTotalAverageTime();
			totalUpdate += perf.Update.GetTotalAverageTime();
			totalUpdateLate += perf.LateUpdate.GetTotalAverageTime();
			totalRender += perf.Render.GetTotalAverageTime();
		}
		Log::Print(Log::LOG_WARN,
			"Total Update Early: %8.3f mcs / %1.3f ms",
			totalUpdateEarly,
			totalUpdateEarly / 1000.0);
		Log::Print(Log::LOG_WARN,
			"Total Update: %8.3f mcs / %1.3f ms",
			totalUpdate,
			totalUpdate / 1000.0);
		Log::Print(Log::LOG_WARN,
			"Total Update Late: %8.3f mcs / %1.3f ms",
			totalUpdateLate,
			totalUpdateLate / 1000.0);
		Log::Print(Log::LOG_WARN,
			"Total Render: %8.3f mcs / %1.3f ms",
			totalRender,
			totalRender / 1000.0);

		Log::Print(Log::LOG_IMPORTANT, "Garbage Size:");
		Log::Print(Log::LOG_INFO, "%u", (Uint32)GarbageCollector::GarbageSize);
	}
}

void Application::SetWindowTitle(const char* title) {
	memset(Application::WindowTitle, 0, sizeof(Application::WindowTitle));
	snprintf(Application::WindowTitle, sizeof(Application::WindowTitle), "%s", title);
	Application::UpdateWindowTitle();
}

void Application::UpdateWindowTitle() {
	std::string titleText = std::string(Application::WindowTitle);

	bool paren = false;

#define ADD_TEXT(text) \
	if (!paren) { \
		paren = true; \
		titleText += " ("; \
	} \
	else \
		titleText += ", "; \
	titleText += text

	if (DevMenu) {
		if (ResourceManager::UsingDataFolder) {
			ADD_TEXT("using Resources folder");
		}
		if (ResourceManager::UsingModPack) {
			ADD_TEXT("using Modpack");
		}
	}

	if (UpdatesPerFrame != 1) {
		ADD_TEXT("Frame Limit OFF");
	}

	switch (Scene::ShowTileCollisionFlag) {
	case 1:
		ADD_TEXT("Viewing Path A");
		break;
	case 2:
		ADD_TEXT("Viewing Path B");
		break;
	}

	if (Stepper) {
		ADD_TEXT("Frame Stepper ON");
	}
#undef ADD_TEXT

	if (paren) {
		titleText += ")";
	}

	SDL_SetWindowTitle(Application::Window, titleText.c_str());
}

void Application::Restart() {
	if (DEBUG_fontSprite) {
		DEBUG_fontSprite->Dispose();
		delete DEBUG_fontSprite;
		DEBUG_fontSprite = NULL;
	}

	// Reset FPS timer
	BenchmarkFrameCount = 0;

	InputManager::ControllerStopRumble();

	Scene::Dispose();
	SceneInfo::Dispose();
	Graphics::DeleteSpriteSheetMap();

	ScriptManager::LoadAllClasses = false;
	ScriptEntity::DisableAutoAnimate = false;

	Graphics::Reset();

	Application::LoadGameConfig();
	Application::LoadGameInfo();
	Application::LoadSceneInfo();
	Application::ReloadSettings();
	Application::DisposeGameConfig();

	FirstFrame = true;
}

void Application::LoadVideoSettings() {
	bool vsyncEnabled;
	Application::Settings->GetBool("display", "vsync", &Graphics::VsyncEnabled);
	Application::Settings->GetInteger("display", "frameSkip", &Application::FrameSkip);

	if (Application::FrameSkip > DEFAULT_MAX_FRAMESKIP){
		Application::FrameSkip = DEFAULT_MAX_FRAMESKIP;
	}

	if (Graphics::Initialized) {
		Graphics::SetVSync(vsyncEnabled);
	}
	else {
		Graphics::VsyncEnabled = vsyncEnabled;

		Application::Settings->GetInteger(
			"display", "multisample", &Graphics::MultisamplingEnabled);
		Application::Settings->GetInteger(
			"display", "defaultMonitor", &Application::DefaultMonitor);
	}
}

#define CLAMP_VOLUME(vol) \
	if (vol < 0) \
		vol = 0; \
	else if (vol > 100) \
	vol = 100

void Application::SetMasterVolume(int volume) {
	CLAMP_VOLUME(volume);

	Application::MasterVolume = volume;
	AudioManager::MasterVolume = Application::MasterVolume / 100.0f;
}
void Application::SetMusicVolume(int volume) {
	CLAMP_VOLUME(volume);

	Application::MusicVolume = volume;
	AudioManager::MusicVolume = Application::MusicVolume / 100.0f;
}
void Application::SetSoundVolume(int volume) {
	CLAMP_VOLUME(volume);

	Application::SoundVolume = volume;
	AudioManager::SoundVolume = Application::SoundVolume / 100.0f;
}

void Application::LoadAudioSettings() {
	INI* settings = Application::Settings;

	int masterVolume = Application::MasterVolume;
	int musicVolume = Application::MusicVolume;
	int soundVolume = Application::SoundVolume;

#define GET_OR_SET_VOLUME(var) \
	if (!settings->PropertyExists("audio", #var)) \
		settings->SetInteger("audio", #var, var); \
	else \
		settings->GetInteger("audio", #var, &var)

	GET_OR_SET_VOLUME(masterVolume);
	GET_OR_SET_VOLUME(musicVolume);
	GET_OR_SET_VOLUME(soundVolume);

#undef GET_OR_SET_VOLUME

	Application::SetMasterVolume(masterVolume);
	Application::SetMusicVolume(musicVolume);
	Application::SetSoundVolume(soundVolume);
}

#undef CLAMP_VOLUME

SDL_Keycode KeyBindsSDL[(int)KeyBind::Max];

void Application::LoadKeyBinds() {
	XMLNode* node = nullptr;
	if (Application::GameConfig) {
		node = XMLParser::SearchNode(Application::GameConfig->children[0], "keys");
	}

#define GET_KEY(setting, bind, def) \
	{ \
		char read[256] = {0}; \
		if (node) { \
			XMLNode* child = XMLParser::SearchNode(node, setting); \
			if (child) { \
				XMLParser::CopyTokenToString( \
					child->children[0]->name, read, sizeof(read)); \
			} \
		} \
		Application::Settings->GetString("keys", setting, read, sizeof(read)); \
		int key = def; \
		if (read[0]) { \
			int parsed = InputManager::ParseKeyName(read); \
			if (parsed >= 0) \
				key = parsed; \
			else \
				key = Key_UNKNOWN; \
		} \
		Application::SetKeyBind((int)KeyBind::bind, key); \
	}

	GET_KEY("fullscreen", Fullscreen, Key_F4);
	GET_KEY("devRestartApp", DevRestartApp, Key_F1);
	GET_KEY("devRestartScene", DevRestartScene, Key_F6);
	GET_KEY("devRecompile", DevRecompile, Key_F5);
	GET_KEY("devPerfSnapshot", DevPerfSnapshot, Key_F3);
	GET_KEY("devLogLayerInfo", DevLayerInfo, Key_F2);
	GET_KEY("devFastForward", DevFastForward, Key_BACKSPACE);
	GET_KEY("devToggleFrameStepper", DevFrameStepper, Key_F9);
	GET_KEY("devStepFrame", DevStepFrame, Key_F10);
	GET_KEY("devShowTileCol", DevTileCol, Key_F7);
	GET_KEY("devShowObjectRegions", DevObjectRegions, Key_F8);
	GET_KEY("devQuit", DevQuit, Key_ESCAPE);

#undef GET_KEY
}

void Application::LoadDevSettings() {
#ifdef DEVELOPER_MODE
	Application::Settings->GetBool("dev", "devMenu", &DevMenu);
	Application::Settings->GetBool("dev", "writeToFile", &Log::WriteToFile);
	Application::Settings->GetBool("dev", "viewPerformance", &ShowFPS);
	Application::Settings->GetBool("dev", "donothing", &DoNothing);
	Application::Settings->GetInteger("dev", "fastforward", &UpdatesPerFastForward);
	Application::Settings->GetBool("dev", "convertModels", &Application::DevConvertModels);
	Application::Settings->GetBool("dev", "useMemoryFileCache", &UseMemoryFileCache);

	int logLevel = 0;
#ifdef DEBUG
	logLevel = -1;
#endif
#ifdef ANDROID
	logLevel = -1;
#endif
	Application::Settings->GetInteger("dev", "logLevel", &logLevel);
	Application::Settings->GetBool("dev", "trackMemory", &Memory::IsTracking);
	Log::SetLogLevel(logLevel);

	Application::Settings->GetBool("dev", "autoPerfSnapshots", &AutomaticPerformanceSnapshots);
	int apsFrameTimeThreshold = 20, apsMinInterval = 5;
	Application::Settings->GetInteger("dev", "apsMinFrameTime", &apsFrameTimeThreshold);
	Application::Settings->GetInteger("dev", "apsMinInterval", &apsMinInterval);
	AutomaticPerformanceSnapshotFrameTimeThreshold = apsFrameTimeThreshold;
	AutomaticPerformanceSnapshotMinInterval = apsMinInterval;

	// The main resource file is not writable by default.
	// This can be enabled by using allowWritableResource.
	bool allowWritableResource = false;
	if (Application::Settings->GetBool(
		    "dev", "allowWritableResource", &allowWritableResource)) {
		if (allowWritableResource) {
			ResourceManager::SetMainResourceWritable(true);
		}
	}
#endif
}

bool Application::IsWindowResizeable() {
	return !Application::IsMobile();
}

void Application::SetWindowSize(int window_w, int window_h) {
	if (!Application::IsWindowResizeable()) {
		return;
	}

	SDL_SetWindowSize(Application::Window, window_w, window_h);

	int defaultMonitor = Application::DefaultMonitor;
	SDL_SetWindowPosition(Application::Window,
		SDL_WINDOWPOS_CENTERED_DISPLAY(defaultMonitor),
		SDL_WINDOWPOS_CENTERED_DISPLAY(defaultMonitor));

	// In case the window just doesn't resize (Android)
	SDL_GetWindowSize(Application::Window, &window_w, &window_h);

	Graphics::Resize(window_w, window_h);
}

bool Application::GetWindowFullscreen() {
	return !!(SDL_GetWindowFlags(Application::Window) & SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void Application::SetWindowFullscreen(bool isFullscreen) {
	SDL_SetWindowFullscreen(
		Application::Window, isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

	int window_w, window_h;
	SDL_GetWindowSize(Application::Window, &window_w, &window_h);

	Graphics::Resize(window_w, window_h);
}

void Application::SetWindowBorderless(bool isBorderless) {
	SDL_SetWindowBordered(Application::Window, (SDL_bool)(!isBorderless));
}

int Application::GetKeyBind(int bind) {
	return KeyBinds[bind];
}
void Application::SetKeyBind(int bind, int key) {
	KeyBinds[bind] = key;
	if (key == Key_UNKNOWN) {
		KeyBindsSDL[bind] = SDLK_UNKNOWN;
	}
	else {
		KeyBindsSDL[bind] = SDL_GetKeyFromScancode(InputManager::KeyToSDLScancode[key]);
	}
}

void Application::PollEvents() {
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_QUIT: {
			Running = false;
			break;
		}
		case SDL_KEYDOWN: {
			SDL_Keycode key = e.key.keysym.sym;

			// Fullscreen
			if (key == KeyBindsSDL[(int)KeyBind::Fullscreen]) {
				Application::SetWindowFullscreen(
					!Application::GetWindowFullscreen());
				break;
			}

			if (DevMenu) {
				// Quit game (dev)
				if (key == KeyBindsSDL[(int)KeyBind::DevQuit]) {
					Running = false;
					// Application::DevMenuActivated
					// ^= 1;
					// Log::Print(Log::LOG_VERBOSE,
					// "Dev Menu Activated: %d",
					// DevMenuActivated);
					break;
				}
				// Restart application (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevRestartApp]) {
					Application::Restart();

					Scene::Init();
					if (*StartingScene) {
						Scene::LoadScene(StartingScene);
					}
					Scene::Restart();
					Application::UpdateWindowTitle();
					break;
				}
				// Show layer info (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevLayerInfo]) {
					for (size_t li = 0; li < Scene::Layers.size(); li++) {
						SceneLayer& layer = Scene::Layers[li];
						Log::Print(Log::LOG_IMPORTANT,
							"%2d: %20s (Visible: %d, Width: %d, Height: %d, OffsetX: %d, OffsetY: %d, RelativeY: %d, ConstantY: %d, DrawGroup: %d, ScrollDirection: %d, Flags: %d)",
							li,
							layer.Name,
							layer.Visible,
							layer.Width,
							layer.Height,
							layer.OffsetX,
							layer.OffsetY,
							layer.RelativeY,
							layer.ConstantY,
							layer.DrawGroup,
							layer.DrawBehavior,
							layer.Flags);
					}
					break;
				}
				// Print performance snapshot (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevPerfSnapshot]) {
					TakeSnapshot = true;
					break;
				}
				// Recompile and restart scene (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevRecompile]) {
					Application::Restart();

					char temp[256];
					memcpy(temp, Scene::CurrentScene, 256);

					Scene::Init();

					memcpy(Scene::CurrentScene, temp, 256);
					Scene::LoadScene(Scene::CurrentScene);

					Scene::Restart();
					Application::UpdateWindowTitle();
					break;
				}
				// Restart scene (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevRestartScene]) {
					// Reset FPS timer
					BenchmarkFrameCount = 0;

					InputManager::ControllerStopRumble();

					Scene::Restart();
					Application::UpdateWindowTitle();
					break;
				}
				// Enable update speedup (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevFastForward]) {
					if (UpdatesPerFrame == 1) {
						UpdatesPerFrame = UpdatesPerFastForward;
					}
					else {
						UpdatesPerFrame = 1;
					}

					Application::UpdateWindowTitle();
					break;
				}
				// Cycle view tile collision (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevTileCol]) {
					Scene::ShowTileCollisionFlag =
						(Scene::ShowTileCollisionFlag + 1) % 3;
					Application::UpdateWindowTitle();
					break;
				}
				// View object regions (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevObjectRegions]) {
					Scene::ShowObjectRegions ^= 1;
					Application::UpdateWindowTitle();
					break;
				}
				// Toggle frame stepper (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevFrameStepper]) {
					Stepper = !Stepper;
					MetricFrameCounterTime = 0;
					Application::UpdateWindowTitle();
					break;
				}
				// Step frame (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevStepFrame]) {
					Stepper = true;
					Step = true;
					MetricFrameCounterTime++;
					Application::UpdateWindowTitle();
					break;
				}
			}
			break;
		}
		case SDL_WINDOWEVENT: {
			switch (e.window.event) {
			case SDL_WINDOWEVENT_RESIZED:
				Graphics::Resize(e.window.data1, e.window.data2);
				break;
			}
			break;
		}
		case SDL_CONTROLLERDEVICEADDED: {
			int i = e.cdevice.which;
			Log::Print(Log::LOG_VERBOSE, "Added controller device %d", i);
			InputManager::AddController(i);
			break;
		}
		case SDL_CONTROLLERDEVICEREMOVED: {
			int i = e.cdevice.which;
			Log::Print(Log::LOG_VERBOSE, "Removed controller device %d", i);
			InputManager::RemoveController(i);
			break;
		}
		}
	}
}
void Application::RunFrameCallback(void* p) {
	RunFrame(UpdatesPerFrame);
}
void Application::RunFrame(int runFrames) {
	FrameTimeStart = Clock::GetTicks();

	// Event loop
	MetricEventTime = Clock::GetTicks();
	Application::PollEvents();
	MetricEventTime = Clock::GetTicks() - MetricEventTime;

	// BUG: Having Stepper on prevents the first
	//   frame of a new scene from Updating, but still rendering.
	if (*Scene::NextScene) {
		Step = true;
	}

	FirstFrame = false;

	MetricAfterSceneTime = Clock::GetTicks();
	Scene::AfterScene();
	MetricAfterSceneTime = Clock::GetTicks() - MetricAfterSceneTime;

	if (DoNothing) {
		goto DO_NOTHING;
	}

	// Update
	for (int m = 0; m < runFrames; m++) {
		Scene::ResetPerf();
		MetricPollTime = 0.0;
		MetricUpdateTime = 0.0;
		if ((Stepper && Step) || !Stepper) {
			// Poll for inputs
			MetricPollTime = Clock::GetTicks();
			InputManager::Poll();
			MetricPollTime = Clock::GetTicks() - MetricPollTime;

			// Update scene
			MetricUpdateTime = Clock::GetTicks();
			Scene::Update();
			MetricUpdateTime = Clock::GetTicks() - MetricUpdateTime;
		}
		Step = false;
		if (runFrames != 1 && (*Scene::NextScene || Scene::DoRestart)) {
			break;
		}
	}

	// Rendering
	MetricClearTime = Clock::GetTicks();
	Graphics::Clear();
	MetricClearTime = Clock::GetTicks() - MetricClearTime;

	MetricRenderTime = Clock::GetTicks();
	Scene::Render();
	MetricRenderTime = Clock::GetTicks() - MetricRenderTime;

DO_NOTHING:

	// Show FPS counter
	MetricFPSCounterTime = Clock::GetTicks();
	if (ShowFPS) {
		if (!DEBUG_fontSprite) {
			bool original = Graphics::TextureInterpolate;
			Graphics::SetTextureInterpolation(true);

			DEBUG_fontSprite = new ISprite();

			int cols, rows;
			Texture* spriteSheet = DEBUG_fontSprite->AddSpriteSheet("Debug/Font.png");
			if (!spriteSheet) {
				spriteSheet = DEBUG_fontSprite->AddSpriteSheet(
					"Sprites/Fonts/DebugFont.png");
			}
			if (spriteSheet) {
				cols = spriteSheet->Width / 32;
				rows = spriteSheet->Height / 32;

				DEBUG_fontSprite->ReserveAnimationCount(1);
				DEBUG_fontSprite->AddAnimation("Font", 0, 0, cols * rows);
				for (int i = 0; i < cols * rows; i++) {
					DEBUG_fontSprite->AddFrame(0,
						(i % cols) * 32,
						(i / cols) * 32,
						32,
						32,
						0,
						0,
						14);
				}
			}
			DEBUG_fontSprite->RefreshGraphicsID();

			Graphics::SetTextureInterpolation(original);
		}

		int ww, wh;
		char textBuffer[256];
		SDL_GetWindowSize(Application::Window, &ww, &wh);
		Graphics::SetViewport(0.0, 0.0, ww, wh);
		Graphics::UpdateOrthoFlipped(ww, wh);

		Graphics::SetBlendMode(BlendFactor_SRC_ALPHA,
			BlendFactor_INV_SRC_ALPHA,
			BlendFactor_SRC_ALPHA,
			BlendFactor_INV_SRC_ALPHA);

		float infoW = 400.0;
		float infoH = 290.0;
		float infoPadding = 20.0;
		Graphics::Save();
		Graphics::Translate(0.0, 0.0, 0.0);
		Graphics::SetBlendColor(0.0, 0.0, 0.0, 0.75);
		Graphics::FillRectangle(0.0f, 0.0f, infoW, infoH);

		double types[] = {
			MetricEventTime,
			MetricAfterSceneTime,
			MetricPollTime,
			MetricUpdateTime,
			MetricClearTime,
			MetricRenderTime,
			MetricPresentTime,
		};
		const char* typeNames[] = {
			"Event Polling: %3.3f ms",
			"Garbage Collector: %3.3f ms",
			"Input Polling: %3.3f ms",
			"Entity Update: %3.3f ms",
			"Clear Time: %3.3f ms",
			"World Render Commands: %3.3f ms",
			"Frame Present Time: %3.3f ms",
		};
		struct {
			float r;
			float g;
			float b;
		} colors[8] = {
			{1.0, 0.0, 0.0},
			{0.0, 1.0, 0.0},
			{0.0, 0.0, 1.0},
			{1.0, 1.0, 0.0},
			{0.0, 1.0, 1.0},
			{1.0, 0.0, 1.0},
			{1.0, 1.0, 1.0},
			{0.0, 0.0, 0.0},
		};

		int typeCount = sizeof(types) / sizeof(double);

		Graphics::Save();
		Graphics::Translate(infoPadding - 2.0, infoPadding, 0.0);
		Graphics::Scale(0.85, 0.85, 1.0);
		snprintf(textBuffer, 256, "Frame Information");
		DEBUG_DrawText(textBuffer, 0.0, 0.0);
		Graphics::Restore();

		Graphics::Save();
		Graphics::Translate(infoW - infoPadding - (8 * 16.0 * 0.85), infoPadding, 0.0);
		Graphics::Scale(0.85, 0.85, 1.0);
		snprintf(textBuffer, 256, "FPS: %03.1f", CurrentFPS);
		DEBUG_DrawText(textBuffer, 0.0, 0.0);
		Graphics::Restore();

		if (Application::Platform == Platforms::Android || true) {
			// Draw bar
			double total = 0.0001;
			for (int i = 0; i < typeCount; i++) {
				if (types[i] < 0.0) {
					types[i] = 0.0;
				}
				total += types[i];
			}

			Graphics::Save();
			Graphics::Translate(infoPadding, 50.0, 0.0);
			Graphics::SetBlendColor(0.0, 0.0, 0.0, 0.25);
			Graphics::FillRectangle(0.0, 0.0f, infoW - infoPadding * 2, 30.0);
			Graphics::Restore();

			double rectx = 0.0;
			for (int i = 0; i < typeCount; i++) {
				Graphics::Save();
				Graphics::Translate(infoPadding, 50.0, 0.0);
				if (i < 8) {
					Graphics::SetBlendColor(
						colors[i].r, colors[i].g, colors[i].b, 0.5);
				}
				else {
					Graphics::SetBlendColor(0.5, 0.5, 0.5, 0.5);
				}
				Graphics::FillRectangle(rectx,
					0.0f,
					types[i] / total * (infoW - infoPadding * 2),
					30.0);
				Graphics::Restore();

				rectx += types[i] / total * (infoW - infoPadding * 2);
			}

			// Draw list
			float listY = 90.0;
			double totalFrameCount = 0.0f;
			infoPadding += infoPadding;
			for (int i = 0; i < typeCount; i++) {
				Graphics::Save();
				Graphics::Translate(infoPadding, listY, 0.0);
				Graphics::SetBlendColor(colors[i].r, colors[i].g, colors[i].b, 0.5);
				Graphics::FillRectangle(-infoPadding / 2.0, 0.0, 12.0, 12.0);
				Graphics::Scale(0.6, 0.6, 1.0);
				snprintf(textBuffer, 256, typeNames[i], types[i]);
				DEBUG_DrawText(textBuffer, 0.0, 0.0);
				listY += 20.0;
				Graphics::Restore();

				totalFrameCount += types[i];
			}

			// Draw total
			Graphics::Save();
			Graphics::Translate(infoPadding, listY, 0.0);
			Graphics::SetBlendColor(1.0, 1.0, 1.0, 0.5);
			Graphics::FillRectangle(-infoPadding / 2.0, 0.0, 12.0, 12.0);
			Graphics::Scale(0.6, 0.6, 1.0);
			snprintf(textBuffer, 256, "Total Frame Time: %.3f ms", totalFrameCount);
			DEBUG_DrawText(textBuffer, 0.0, 0.0);
			listY += 20.0;
			Graphics::Restore();

			// Draw Overdelay
			Graphics::Save();
			Graphics::Translate(infoPadding, listY, 0.0);
			Graphics::SetBlendColor(1.0, 1.0, 1.0, 0.5);
			Graphics::FillRectangle(-infoPadding / 2.0, 0.0, 12.0, 12.0);
			Graphics::Scale(0.6, 0.6, 1.0);
			snprintf(textBuffer, 256, "Overdelay: %.3f ms", Overdelay);
			DEBUG_DrawText(textBuffer, 0.0, 0.0);
			listY += 20.0;
			Graphics::Restore();

			float count = (float)Memory::MemoryUsage;
			const char* moniker = "B";

			if (count >= 1000000000) {
				count /= 1000000000;
				moniker = "GB";
			}
			else if (count >= 1000000) {
				count /= 1000000;
				moniker = "MB";
			}
			else if (count >= 1000) {
				count /= 1000;
				moniker = "KB";
			}

			listY += 30.0 - 20.0;

			Graphics::Save();
			Graphics::Translate(infoPadding / 2.0, listY, 0.0);
			Graphics::Scale(0.6, 0.6, 1.0);
			snprintf(textBuffer, 256, "RAM Usage: %.3f %s", count, moniker);
			DEBUG_DrawText(textBuffer, 0.0, 0.0);
			Graphics::Restore();

			listY += 30.0;

			float* listYPtr = &listY;
			if (Scene::ObjectLists && Application::Platform != Platforms::Android) {
				Scene::ObjectLists->WithAll([infoPadding, listYPtr](Uint32,
								    ObjectList* list) -> void {
					char textBufferXXX[1024];
					if (list->Performance.Update.AverageItemCount > 0.0) {
						Graphics::Save();
						Graphics::Translate(
							infoPadding / 2.0, *listYPtr, 0.0);
						Graphics::Scale(0.6, 0.6, 1.0);
						snprintf(textBufferXXX,
							1024,
							"Object \"%s\": Avg Render %.1f mcs (Total %.1f mcs, Count %d)",
							list->ObjectName,
							list->Performance.Render.GetAverageTime(),
							list->Performance.Render
								.GetTotalAverageTime(),
							(int)list->Performance.Render
								.AverageItemCount);
						DEBUG_DrawText(textBufferXXX, 0.0, 0.0);
						Graphics::Restore();

						*listYPtr += 20.0;
					}
				});
			}
		}
		Graphics::Restore();
	}
	MetricFPSCounterTime = Clock::GetTicks() - MetricFPSCounterTime;

	MetricPresentTime = Clock::GetTicks();
	Graphics::Present();
	MetricPresentTime = Clock::GetTicks() - MetricPresentTime;

	MetricFrameTime = Clock::GetTicks() - FrameTimeStart;
}
void Application::DelayFrame() {
	double frameTime = Clock::GetTicks() - FrameTimeStart;
	double frameDurationRemainder = FrameTimeDesired - frameTime;
	if (frameDurationRemainder >= 0.0) {
		// NOTE: Delay duration will always be more than
		// requested wait time.
		if (frameDurationRemainder > 1.0) {
			double delayStartTime = Clock::GetTicks();

			Clock::Delay(frameDurationRemainder - 1.0);

			double delayTime = Clock::GetTicks() - delayStartTime;
			Overdelay = delayTime - (frameDurationRemainder - 1.0);
		}

		while ((Clock::GetTicks() - FrameTimeStart) < FrameTimeDesired)
			;
	}
}
void Application::Run(int argc, char* args[]) {
	Application::Init(argc, args);
	if (!Running) {
		return;
	}

	Scene::Init();

	if (argc > 1 && AllowCmdLineSceneLoad) {
		char* pathStart = StringUtils::StrCaseStr(args[1], "/Resources/");
		if (pathStart == NULL) {
			pathStart = StringUtils::StrCaseStr(args[1], "\\Resources\\");
		}

		if (pathStart) {
			char* tmxPath = pathStart + strlen("/Resources/");
			for (char* i = tmxPath; *i; i++) {
				if (*i == '\\') {
					*i = '/';
				}
			}
			Scene::LoadScene(tmxPath);
		}
		else {
			Log::Print(Log::LOG_WARN,
				"Map file \"%s\" not inside Resources folder!",
				args[1]);
		}
	}
	else if (*StartingScene) {
		Scene::LoadScene(StartingScene);
	}

	Scene::Restart();
	Application::UpdateWindowTitle();
	Application::SetWindowSize(Application::WindowWidth, Application::WindowHeight);

	Graphics::Clear();
	Graphics::Present();

	SDL_ShowWindow(Application::Window);

#ifdef IOS
	// Initialize the Game Center for scoring and matchmaking
	// InitGameCenter();

	// Set up the game to run in the window animation callback on
	// iOS so that Game Center and so forth works correctly.
	SDL_iPhoneSetAnimationCallback(Application::Window, 1, RunFrameCallback, NULL);
#else
	float lastTick = Clock::GetTicks();
	while (Running) {
		float tickStart = Clock::GetTicks();
		float timeTaken = tickStart - lastTick;
		lastTick = tickStart;

		int updateFrames = UpdatesPerFrame;
		if (updateFrames == 1) {
			// Compensate for lag
			int lagFrames = ( (int) round(timeTaken / FrameTimeDesired) ) - 1;
			if (lagFrames > Application::FrameSkip){
				lagFrames = Application::FrameSkip;
			}

			if (!FirstFrame && lagFrames > 0) {
				updateFrames += lagFrames;
			}

		}

		if (BenchmarkFrameCount == 0) {
			BenchmarkTickStart = tickStart;
		}

		Application::RunFrame(updateFrames);
		Application::DelayFrame();

		// Do benchmarking stuff
		BenchmarkFrameCount++;
		if (BenchmarkFrameCount == TargetFPS) {
			double measuredSecond = Clock::GetTicks() - BenchmarkTickStart;
			CurrentFPS = 1000.0 / floor(measuredSecond) * TargetFPS;
			BenchmarkFrameCount = 0;
		}

		if (AutomaticPerformanceSnapshots &&
			MetricFrameTime > AutomaticPerformanceSnapshotFrameTimeThreshold) {
			if (Clock::GetTicks() - AutomaticPerformanceSnapshotLastTime >
				AutomaticPerformanceSnapshotMinInterval) {
				AutomaticPerformanceSnapshotLastTime = Clock::GetTicks();
				TakeSnapshot = true;
			}
		}

		if (TakeSnapshot) {
			TakeSnapshot = false;
			Application::GetPerformanceSnapshot();
		}
	}

	Scene::Dispose();

	Application::Cleanup();
#endif
}

void Application::Cleanup() {
	if (DEBUG_fontSprite) {
		DEBUG_fontSprite->Dispose();
		delete DEBUG_fontSprite;
		DEBUG_fontSprite = NULL;
	}

	MemoryCache::Dispose();
	ResourceManager::Dispose();
	AudioManager::Dispose();
	InputManager::Dispose();

	Graphics::Dispose();

	for (size_t i = 0; i < Application::CmdLineArgs.size(); i++) {
		Memory::Free(Application::CmdLineArgs[i]);
	}
	Application::CmdLineArgs.clear();

	Memory::PrintLeak();
	Memory::ClearTrackedMemory();

	MemoryPools::Dispose();

	Log::Close();

	SDL_DestroyWindow(Application::Window);

	SDL_Quit();

#ifdef MSYS
	FreeConsole();
#endif
}

static char* ParseGameConfigText(XMLNode* parent, const char* option) {
	XMLNode* node = XMLParser::SearchNode(parent, option);
	if (!node) {
		return nullptr;
	}

	return XMLParser::TokenToString(node->children[0]->name);
}
static bool ParseGameConfigText(XMLNode* parent, const char* option, char* buf, size_t bufSize) {
	XMLNode* node = XMLParser::SearchNode(parent, option);
	if (!node) {
		return false;
	}

	XMLParser::CopyTokenToString(node->children[0]->name, buf, bufSize);

	return true;
}
static bool ParseGameConfigInt(XMLNode* parent, const char* option, int& val) {
	XMLNode* node = XMLParser::SearchNode(parent, option);
	if (!node) {
		return false;
	}

	char read[32];
	XMLParser::CopyTokenToString(node->children[0]->name, read, sizeof(read));
	return StringUtils::ToNumber(&val, read);
}
static bool ParseGameConfigBool(XMLNode* node, const char* option, bool& val) {
	node = XMLParser::SearchNode(node, option);
	if (!node) {
		return false;
	}

	char read[5];
	XMLParser::CopyTokenToString(node->children[0]->name, read, sizeof(read));
	val = !strcmp(read, "true");

	return true;
}

void Application::LoadGameConfig() {
	StartingScene[0] = '\0';
	LogFilename[0] = '\0';

	Application::GameConfig = nullptr;

	if (!ResourceManager::ResourceExists("GameConfig.xml")) {
		return;
	}

	Application::GameConfig = XMLParser::ParseFromResource("GameConfig.xml");
	if (!Application::GameConfig) {
		return;
	}

	XMLNode* root = Application::GameConfig->children[0];
	XMLNode* node;

	// Read engine settings
	node = XMLParser::SearchNode(root, "engine");
	if (node) {
		int targetFPS = DEFAULT_TARGET_FRAMERATE;
		ParseGameConfigInt(node, "framerate", targetFPS);
		Application::SetTargetFrameRate(targetFPS);

		ParseGameConfigBool(
			node, "allowCmdLineSceneLoad", Application::AllowCmdLineSceneLoad);
		ParseGameConfigBool(node, "loadAllClasses", ScriptManager::LoadAllClasses);
		ParseGameConfigBool(node, "useSoftwareRenderer", Graphics::UseSoftwareRenderer);
		ParseGameConfigBool(node, "enablePaletteUsage", Graphics::UsePalettes);
		ParseGameConfigBool(node, "portableMode", Application::PortableMode);
		ParseGameConfigBool(node, "writeLogFile", Log::WriteToFile);
		ParseGameConfigText(node, "logFilename", LogFilename, sizeof LogFilename);
	}

	// Read display defaults
	node = XMLParser::SearchNode(root, "display");
	if (node) {
		ParseGameConfigInt(node, "width", Application::WindowWidth);
		ParseGameConfigInt(node, "height", Application::WindowHeight);
	}

	// Read audio defaults
#define GET_VOLUME(node, func) \
	if (node->attributes.Exists("volume")) { \
		int volume; \
		char xmlTokStr[32]; \
		XMLParser::CopyTokenToString( \
			node->attributes.Get("volume"), xmlTokStr, sizeof(xmlTokStr)); \
		if (StringUtils::ToNumber(&volume, xmlTokStr)) \
			Application::func(volume); \
	}
	node = XMLParser::SearchNode(root, "audio");
	if (node) {
		// Get master audio volume
		GET_VOLUME(node, SetMasterVolume);

		XMLNode* parent = node;

		// Get music volume
		node = XMLParser::SearchNode(parent, "music");
		if (node) {
			GET_VOLUME(node, SetMusicVolume);
		}

		// Get sound volume
		node = XMLParser::SearchNode(parent, "sound");
		if (node) {
			GET_VOLUME(node, SetSoundVolume);
		}
	}
#undef GET_VOLUME

	// Read starting scene
	node = XMLParser::SearchNode(root, "startscene");
	if (node) {
		XMLParser::CopyTokenToString(
			node->children[0]->name, StartingScene, sizeof(StartingScene));
	}
}

void Application::DisposeGameConfig() {
	if (Application::GameConfig) {
		XMLParser::Free(Application::GameConfig);
	}
	Application::GameConfig = nullptr;
}

string Application::ParseGameVersion(XMLNode* versionNode) {
	if (versionNode->children.size() == 1) {
		return versionNode->children[0]->name.ToString();
	}

	std::string versionText = "";

	// major
	XMLNode* node = XMLParser::SearchNode(versionNode, "major");
	if (node) {
		versionText += node->children[0]->name.ToString();
	}
	else {
		return versionText;
	}

	// minor
	node = XMLParser::SearchNode(versionNode, "minor");
	if (node) {
		versionText += "." + node->children[0]->name.ToString();
	}
	else {
		return versionText;
	}

	// patch
	node = XMLParser::SearchNode(versionNode, "patch");
	if (node) {
		versionText += "." + node->children[0]->name.ToString();
	}
	else {
		return versionText;
	}

	// pre-release
	node = XMLParser::SearchNode(versionNode, "prerelease");
	if (node) {
		versionText += "-" + node->children[0]->name.ToString();
	}

	return versionText;
}

void Application::LoadGameInfo() {
	StringUtils::Copy(
		Application::GameTitle, "Hatch Game Engine", sizeof(Application::GameTitle));
	StringUtils::Copy(Application::GameTitleShort,
		Application::GameTitle,
		sizeof(Application::GameTitleShort));
	StringUtils::Copy(Application::GameVersion, "1.0", sizeof(Application::GameVersion));
	StringUtils::Copy(Application::GameDescription,
		"Cluck cluck I'm a chicken",
		sizeof(Application::GameDescription));

	if (Application::GameConfig) {
		XMLNode* root = Application::GameConfig->children[0];

		XMLNode* node = XMLParser::SearchNode(root, "gameTitle");
		if (node == nullptr) {
			node = XMLParser::SearchNode(root, "name");
		}
		if (node) {
			XMLParser::CopyTokenToString(node->children[0]->name,
				Application::GameTitle,
				sizeof(Application::GameTitle));
			StringUtils::Copy(Application::GameTitleShort,
				Application::GameTitle,
				sizeof(Application::GameTitleShort));
		}

		node = XMLParser::SearchNode(root, "shortTitle");
		if (node) {
			XMLParser::CopyTokenToString(node->children[0]->name,
				Application::GameTitleShort,
				sizeof(Application::GameTitleShort));
		}

		node = XMLParser::SearchNode(root, "version");
		if (node) {
			std::string versionText = Application::ParseGameVersion(node);
			if (versionText.size() > 0) {
				StringUtils::Copy(Application::GameVersion,
					versionText.c_str(),
					sizeof(Application::GameVersion));
			}
		}

		node = XMLParser::SearchNode(root, "description");
		if (node) {
			XMLParser::CopyTokenToString(node->children[0]->name,
				Application::GameDescription,
				sizeof(Application::GameDescription));
		}
	}
}

void Application::LoadSceneInfo() {
	XMLNode* sceneConfig = nullptr;

	int startSceneNum = 0;

	Scene::ActiveCategory = 0;
	Scene::CurrentSceneInList = 0;

	// Open and read SceneConfig
	if (ResourceManager::ResourceExists("Game/SceneConfig.xml")) {
		sceneConfig = XMLParser::ParseFromResource("Game/SceneConfig.xml");
	}
	else if (ResourceManager::ResourceExists("SceneConfig.xml")) {
		sceneConfig = XMLParser::ParseFromResource("SceneConfig.xml");
	}

	// Parse Scene List
	if (sceneConfig) {
		if (SceneInfo::Load(sceneConfig->children[0])) {
			// Read category and starting scene number to
			// be used by the SceneConfig
			if (Application::GameConfig) {
				XMLNode* node = Application::GameConfig->children[0];
				if (node) {
					// Parse active category
					if (!ParseGameConfigInt(node,
						    "activeCategory",
						    Scene::ActiveCategory)) { // backwards compat
						char* text =
							ParseGameConfigText(node, "activeCategory");
						if (text) {
							int id = SceneInfo::GetCategoryID(text);
							if (id >= 0) {
								Scene::ActiveCategory = id;
							}
							Memory::Free(text);
						}
					}

					// Parse starting scene
					ParseGameConfigInt(node,
						"startSceneNum",
						startSceneNum); // backwards
					// compat

					char* text = ParseGameConfigText(node, "startscene");
					if (text) {
						int id = SceneInfo::GetEntryID(
							Scene::ActiveCategory, text);
						if (id >= 0) {
							startSceneNum = id;
						}
						Memory::Free(text);
					}
				}
			}

			// TODO: Check existing scene folder and id
			// here to reset them upon reload
			if (SceneInfo::IsEntryValid(Scene::ActiveCategory, startSceneNum)) {
				Scene::CurrentSceneInList = startSceneNum;
			}

			if (StartingScene[0] == '\0' &&
				SceneInfo::CategoryHasEntries(Scene::ActiveCategory)) {
				Scene::SetInfoFromCurrentID();

				StringUtils::Copy(StartingScene,
					SceneInfo::GetFilename(
						Scene::ActiveCategory, Scene::CurrentSceneInList)
						.c_str(),
					sizeof(StartingScene));
			}

			Log::Print(Log::LOG_VERBOSE,
				"Loaded scene list (%d categories, %d scenes)",
				SceneInfo::Categories.size(),
				SceneInfo::NumTotalScenes);
		}

		XMLParser::Free(sceneConfig);
	}
}

void Application::InitPlayerControls() {
	InputManager::InitPlayerControls();
}

bool Application::LoadSettings(const char* filename) {
	INI* ini = INI::Load(filename);
	if (ini == nullptr) {
		return false;
	}

	StringUtils::Copy(Application::SettingsFile, filename, sizeof(Application::SettingsFile));

	if (Application::Settings) {
		Application::Settings->Dispose();
	}
	Application::Settings = ini;

	return true;
}

void Application::ReadSettings() {
	Application::LoadVideoSettings();
	Application::LoadAudioSettings();
	Application::LoadDevSettings();
	Application::LoadKeyBinds();
}

void Application::ReloadSettings() {
	// First time load
	if (Application::Settings == nullptr) {
		Application::InitSettings(DEFAULT_SETTINGS_FILENAME);
	}

	if (Application::Settings->Reload()) {
		Application::ReadSettings();
	}
}

void Application::ReloadSettings(const char* filename) {
	if (Application::LoadSettings(filename)) {
		Application::ReadSettings();
	}
}

void Application::InitSettings(const char* filename) {
	// Create settings with default values.
	StringUtils::Copy(Application::SettingsFile, filename, sizeof(Application::SettingsFile));

	Application::Settings = INI::New(Application::SettingsFile);

	Application::Settings->SetBool("display", "fullscreen", false);
	Application::Settings->SetBool("display", "vsync", false);
	Application::Settings->SetInteger("display", "frameSkip", DEFAULT_MAX_FRAMESKIP);
}
void Application::SaveSettings() {
	if (Application::Settings) {
		Application::Settings->Save();
	}
}
void Application::SaveSettings(const char* filename) {
	if (Application::Settings) {
		Application::Settings->Save(filename);
	}
}
void Application::SetSettingsFilename(const char* filename) {
	StringUtils::Copy(Application::SettingsFile, filename, sizeof(Application::SettingsFile));
	if (Application::Settings) {
		Application::Settings->SetFilename(filename);
	}
}

int Application::HandleAppEvents(void* data, SDL_Event* event) {
	switch (event->type) {
	case SDL_APP_TERMINATING:
		Log::Print(Log::LOG_VERBOSE, "SDL_APP_TERMINATING");
		Scene::OnEvent(event->type);
		return 0;
	case SDL_APP_LOWMEMORY:
		Log::Print(Log::LOG_VERBOSE, "SDL_APP_LOWMEMORY");
		Scene::OnEvent(event->type);
		return 0;
	case SDL_APP_WILLENTERBACKGROUND:
		Log::Print(Log::LOG_VERBOSE, "SDL_APP_WILLENTERBACKGROUND");
		Scene::OnEvent(event->type);
		return 0;
	case SDL_APP_DIDENTERBACKGROUND:
		Log::Print(Log::LOG_VERBOSE, "SDL_APP_DIDENTERBACKGROUND");
		Scene::OnEvent(event->type);
		return 0;
	case SDL_APP_WILLENTERFOREGROUND:
		Log::Print(Log::LOG_VERBOSE, "SDL_APP_WILLENTERFOREGROUND");
		Scene::OnEvent(event->type);
		return 0;
	case SDL_APP_DIDENTERFOREGROUND:
		Log::Print(Log::LOG_VERBOSE, "SDL_APP_DIDENTERFOREGROUND");
		Scene::OnEvent(event->type);
		return 0;
	default:
		return 1;
	}
}

