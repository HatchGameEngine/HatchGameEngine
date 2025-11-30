#include <Engine/Application.h>
#include <Engine/Graphics.h>

#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Data/DefaultFonts.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Diagnostics/MemoryPools.h>
#include <Engine/Diagnostics/PerformanceViewer.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
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

vector<std::string> Application::CmdLineArgs;

INI* Application::Settings = NULL;
char Application::SettingsFile[MAX_PATH_LENGTH];

XMLNode* Application::GameConfig = NULL;
Font* Application::DefaultFont = NULL;

int Application::TargetFPS = DEFAULT_TARGET_FRAMERATE;
float Application::CurrentFPS = DEFAULT_TARGET_FRAMERATE;
bool Application::Running = false;
bool Application::FirstFrame = true;
bool Application::ShowFPS = false;

SDL_Window* Application::Window = NULL;
char Application::WindowTitle[256];
int Application::WindowWidth = 424;
int Application::WindowHeight = 240;
int Application::WindowScale = 2;
bool Application::WindowFullscreen = false;
bool Application::WindowBorderless = false;

int Application::DefaultMonitor = 0;

char Application::EngineVersion[256];

char Application::GameTitle[256];
char Application::GameTitleShort[256];
char Application::GameVersion[256];
char Application::GameDescription[256];
char Application::GameDeveloper[256];

char Application::GameIdentifier[256];
char Application::DeveloperIdentifier[256];
char Application::SavesDir[256];
char Application::PreferencesDir[256];

std::unordered_map<std::string, Capability> Application::CapabilityMap;
std::vector<std::string> Application::DefaultFontList;

int Application::UpdatesPerFrame = 1;
int Application::FrameSkip = DEFAULT_MAX_FRAMESKIP;
bool Application::Stepper = false;
bool Application::Step = false;

int Application::MasterVolume = 100;
int Application::MusicVolume = 100;
int Application::SoundVolume = 100;

bool Application::DisableDefaultActions = false;
bool Application::DevMenuActivated = false;
DeveloperMenu Application::DevMenu;

bool Application::DevConvertModels = false;

bool Application::AllowCmdLineSceneLoad = false;

ApplicationMetrics Application::Metrics;
std::vector<PerformanceMeasure*> Application::AllMetrics;

char StartingScene[MAX_RESOURCE_PATH_LENGTH];
char NextGame[MAX_PATH_LENGTH];
char NextGameStartingScene[MAX_RESOURCE_PATH_LENGTH];
std::vector<std::string>* NextGameCmdLineArgs;
char LogFilename[MAX_PATH_LENGTH];

bool UseMemoryFileCache = false;

bool DevMode = false;
bool ViewPerformance = false;
bool TakeSnapshot = false;
bool DoNothing = false;
int UpdatesPerFastForward = 4;

bool AutomaticPerformanceSnapshots;
double AutomaticPerformanceSnapshotFrameTimeThreshold;
double AutomaticPerformanceSnapshotLastTime;
double AutomaticPerformanceSnapshotMinInterval;

int BenchmarkFrameCount = 0;
double BenchmarkTickStart = 0.0;

double Overdelay = 0.0;
double FrameTimeStart = 0.0;
double FrameTimeDesired = 1000.0 / Application::TargetFPS;

int KeyBinds[(int)KeyBind::Max];

void Application::Init(int argc, char* args[]) {
#ifdef MSYS
	AllocConsole();
	freopen_s((FILE**)stdin, "CONIN$", "w", stdin);
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
#endif

	Application::MakeEngineVersion();

	Log::Init();

	MemoryPools::Init();

	Application::InitPerformanceMetrics();

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
		Application::CmdLineArgs.push_back(std::string(args[i]));
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
	Application::InitGameInfo();
	Application::LoadGameInfo();
	Application::ReloadSettings();

	// Open the log file immediately after
	Log::OpenFile(LogFilename);

	Application::LogEngineVersion();
	Application::LogSystemInfo();

	// Keep loading game stuff.
	Application::LoadSceneInfo(0, 0, false);
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

	Application::SaveSettings();

	AudioManager::Init();

	Running = true;
}
void Application::InitScripting() {
	GarbageCollector::Init();

	Compiler::Init();

	ScriptManager::Init();
	ScriptManager::ResetStack();
	ScriptManager::LinkStandardLibrary();
	ScriptManager::LinkExtensions();

	Compiler::GetStandardConstants();

	if (SourceFileMap::CheckForUpdate()) {
		ScriptManager::ForceGarbageCollection();
	}
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

	int scale = 2;
	Application::Settings->GetInteger("display", "scale", &scale);
	Application::SetWindowScale(scale);

	Application::Window = SDL_CreateWindow(NULL,
		SDL_WINDOWPOS_CENTERED_DISPLAY(defaultMonitor), SDL_WINDOWPOS_CENTERED_DISPLAY(defaultMonitor),
		Application::WindowWidth * Application::WindowScale, Application::WindowHeight * Application::WindowScale, window_flags);


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
		Application::Settings->GetBool("display", "fullscreen", &Application::WindowFullscreen);

		if (Application::GetWindowFullscreen() != Application::WindowFullscreen)
			Application::SetWindowFullscreen(Application::WindowFullscreen);
	}

	Application::Settings->GetBool("display", "borderless", &Application::WindowBorderless);
	Application::SetWindowBorderless(Application::WindowBorderless);

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

void Application::AddCapability(std::string capability, int value) {
	RemoveCapability(capability);

	Application::CapabilityMap[capability] = Capability(value);
}
void Application::AddCapability(std::string capability, float value) {
	RemoveCapability(capability);

	Application::CapabilityMap[capability] = Capability(value);
}
void Application::AddCapability(std::string capability, bool value) {
	RemoveCapability(capability);

	Application::CapabilityMap[capability] = Capability(value);
}
void Application::AddCapability(std::string capability, std::string value) {
	char* string = StringUtils::Create(value);

	RemoveCapability(capability);

	Application::CapabilityMap[capability] = Capability(string);
}
Capability Application::GetCapability(std::string capability) {
	std::unordered_map<std::string, Capability>::iterator it = CapabilityMap.find(capability);
	if (it != CapabilityMap.end()) {
		return it->second;
	}

	return Capability();
}
bool Application::HasCapability(std::string capability) {
	std::unordered_map<std::string, Capability>::iterator it = CapabilityMap.find(capability);
	return it != CapabilityMap.end();
}
void Application::RemoveCapability(std::string capability) {
	if (HasCapability(capability)) {
		Capability cap = CapabilityMap[capability];

		cap.Dispose();

		CapabilityMap.erase(capability);
	}
}

bool IsIdentifierBody(char c) {
	switch (c) {
	case '.':
	case '-':
	case '_':
	case ' ':
	case '\'':
	case '!':
	case '@':
	case '&':
		return true;
	default:
		return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
	}
}

bool Application::ValidateIdentifier(const char* string) {
	if (string == nullptr || string[0] == '\0') {
		return false;
	}

	// Cannot start with a dot, a hyphen, or space
	if (string[0] == '.' || string[0] == '-' || string[0] == ' ') {
		return false;
	}

	const char* ptr = string;
	while (*ptr != '\0') {
		char c = *ptr;

		if (IsIdentifierBody(c)) {
			ptr++;
			continue;
		}

		return false;
	}

	// Cannot end with a space
	if ((unsigned char)(*(ptr - 1)) == ' ') {
		return false;
	}

	return true;
}

char* Application::GenerateIdentifier(const char* string) {
	if (string == nullptr || string[0] == '\0') {
		return nullptr;
	}

	// Prohibit '.' or '..'
	if (strcmp(string, ".") == 0 || strcmp(string, "..") == 0) {
		return nullptr;
	}

	char* buf = (char*)Memory::Malloc(strlen(string) + 1);
	size_t i = 0;

	const char* ptr = string;
	if (*ptr == '-' || *ptr == ' ') {
		ptr++;
	}
	else {
		while (*ptr == '.') {
			ptr++;
		}
	}

	while (*ptr != '\0') {
		char c = *ptr;
		if (IsIdentifierBody(c)) {
			buf[i++] = c;
		}
		ptr++;
	}

	if (i == 0) {
		Memory::Free(buf);
		return nullptr;
	}

	// Cannot end with a space
	i--;

	while (buf[i] == ' ') {
		if (i == 0) {
			Memory::Free(buf);
			return nullptr;
		}
		i--;
	}

	i++;

	buf[i] = '\0';

	return (char*)Memory::Realloc(buf, i + 1);
}

bool Application::ValidateAndSetIdentifier(const char* name,
	const char* id,
	char* dest,
	size_t destSize) {
	if (Application::ValidateIdentifier(id)) {
		StringUtils::Copy(dest, id, destSize);
		return true;
	}

	Log::Print(Log::LOG_WARN, "Not a valid %s: %s", name, id);

	return false;
}

// Returns a "safe" version of the developer's name (for e.g. file names)
const char* Application::GetDeveloperIdentifier() {
	if (DeveloperIdentifier[0] == '\0') {
		return nullptr;
	}

	return DeveloperIdentifier;
}

// Returns a "safe" version of the game's name (for e.g. file names)
const char* Application::GetGameIdentifier() {
	if (GameIdentifier[0] == '\0') {
		return nullptr;
	}

	return GameIdentifier;
}

// Returns the name of the saves directory
const char* Application::GetSavesDir() {
	if (SavesDir[0] == '\0') {
		return nullptr;
	}

	return SavesDir;
}

// Returns the name of the preferences directory
const char* Application::GetPreferencesDir() {
	if (PreferencesDir[0] == '\0') {
		return nullptr;
	}

	return PreferencesDir;
}

void Application::LoadDefaultFont() {
	std::vector<Stream*> streamList;
	bool hadError = false;

	for (size_t i = 0; i < DefaultFontList.size(); i++) {
		const char* filename = DefaultFontList[i].c_str();
		Stream* stream = ResourceStream::New(filename);
		if (stream) {
			streamList.push_back(stream);
		}
		else {
			Log::Print(Log::LOG_ERROR, "Resource \"%s\" does not exist!", filename);
			hadError = true;
		}
	}

	if (streamList.size() == 0) {
		void* data = GetDefaultFontData();
		if (data == nullptr) {
			Application::UnloadDefaultFont();
			return;
		}

		if (hadError) {
			Log::Print(Log::LOG_IMPORTANT, "Loading the default font as a fallback.");
		}

		MemoryStream* stream = MemoryStream::New(data, GetDefaultFontDataLength());
		if (stream) {
			streamList.push_back(stream);
		}
	}

	if (streamList.size() == 0) {
		Log::Print(Log::LOG_WARN, "No default fonts to load!");
		return;
	}

	Application::UnloadDefaultFont();

	int oversampling = 1;
	Application::Settings->GetInteger("graphics", "defaultFontOversampling", &oversampling);

	DefaultFont = new Font();
	DefaultFont->SetOversampling(oversampling);

	if (DefaultFont->Load(streamList) && DefaultFont->LoadSize(DEFAULT_FONT_SIZE)) {
		DefaultFont->LoadFailed = false;
	}
	else {
		Log::Print(Log::LOG_WARN, "Couldn't load the default font!");
		Application::UnloadDefaultFont();
	}

	for (size_t i = 0; i < streamList.size(); i++) {
		streamList[i]->Close();
	}
}

void Application::UnloadDefaultFont() {
	if (DefaultFont) {
		delete DefaultFont;
		DefaultFont = nullptr;
	}
}

void Application::GetPerformanceSnapshot() {
	// General Performance Snapshot
	Log::Print(Log::LOG_IMPORTANT, "General Performance Snapshot:");
	for (size_t i = 0; i < Application::AllMetrics.size(); i++) {
		PerformanceMeasure* measure = Application::AllMetrics[i];

		char timeString[64];
		snprintf(timeString, sizeof timeString, "%.3f ms", measure->Time);

		char padStr[32 + 1];
		int padding = (sizeof(padStr) - 1) - strlen(measure->Name) + 1 - strlen(timeString);
		if (padding < 1) {
			padding = 1;
		}
		for (int i = 0; i < padding; i++) {
			padStr[i] = ' ';
		}
		padStr[padding] = '\0';

		Log::Print(Log::LOG_INFO, "%s:%s%s", measure->Name, padStr, timeString);
	}
	Log::Print(Log::LOG_INFO, "FPS: %27.3f", CurrentFPS);

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
				tilesTotal += Scene::PERF_ViewRender[i].LayerTileRenderTime[li];
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
	vector<ObjectList*> objListPerf = Scene::GetObjectListPerformance();
	if (objListPerf.size() > 0) {
		double totalUpdateEarly = 0.0;
		double totalUpdate = 0.0;
		double totalUpdateLate = 0.0;
		double totalRender = 0.0;
		Log::Print(Log::LOG_IMPORTANT, "Object Performance Snapshot:");
		for (size_t i = 0; i < objListPerf.size(); i++) {
			ObjectList* list = objListPerf[i];
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
	}

	Log::Print(Log::LOG_IMPORTANT, "Garbage Size: %u", (Uint32)GarbageCollector::GarbageSize);
}
double Application::GetOverdelay() {
	return Overdelay;
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

	if (DevMode) {
		if (ResourceManager::UsingDataFolder) {
			ADD_TEXT("using Resources folder");
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

void Application::EndGame() {
	Application::UnloadDefaultFont();
	Application::DefaultFontList.clear();

	// Reset FPS timer
	BenchmarkFrameCount = 0;

	InputManager::ControllerStopRumble();

	Scene::Dispose();
	SceneInfo::Dispose();
	Graphics::UnloadData();

	Application::TerminateScripting();
}

void Application::UnloadGame() {
	Application::EndGame();

	MemoryCache::Dispose();
	ResourceManager::Dispose();

	InputManager::ClearPlayers();
	InputManager::ClearInputs();
}

void Application::Restart(bool keepScene) {
	Application::EndGame();

	Graphics::Reset();

	Application::LoadGameConfig();
	Application::InitGameInfo();
	Application::LoadGameInfo();
	Application::ReloadSettings();
	keepScene ? Application::LoadSceneInfo(Scene::ActiveCategory, Scene::CurrentSceneInList, true) : Application::LoadSceneInfo(0, 0, false);
	Application::DisposeGameConfig();

	FirstFrame = true;
}

bool Application::ChangeGame(const char* path) {
	char startingScene[MAX_PATH_LENGTH];
	char lastSettingsPath[MAX_PATH_LENGTH];
	char currentSettingsPath[MAX_PATH_LENGTH];

	Path::FromURL(SettingsFile, lastSettingsPath, sizeof lastSettingsPath);

	if (NextGameStartingScene[0] != '\0') {
		StringUtils::Copy(startingScene, NextGameStartingScene, sizeof startingScene);
		NextGameStartingScene[0] = '\0';
	}
	else {
		startingScene[0] = '\0';
	}

	Application::UnloadGame();

	if (!ResourceManager::Init(path)) {
		if (NextGameCmdLineArgs) {
			delete NextGameCmdLineArgs;
			NextGameCmdLineArgs = nullptr;
		}

		return false;
	}

	Application::LoadGameConfig();
	Application::LoadGameInfo();

	// Reload settings file if the path to it changed.
	Path::FromURL(SettingsFile, currentSettingsPath, sizeof currentSettingsPath);
	if (strcmp(currentSettingsPath, lastSettingsPath) != 0) {
		Application::DisposeSettings();

		if (!Application::LoadSettings(Application::SettingsFile)) {
			// Couldn't load new settings file, so use a default one.
			Application::InitSettings();
		}
	}

	if (UseMemoryFileCache) {
		MemoryCache::Init();
	}

	Application::LoadSceneInfo(0, 0, false);
	Application::InitPlayerControls();
	Application::DisposeGameConfig();

	FirstFrame = true;

	SourceFileMap::AllowCompilation = false;

	if (startingScene[0] == '\0') {
		StringUtils::Copy(startingScene, StartingScene, sizeof startingScene);
	}

	if (NextGameCmdLineArgs) {
		Application::CmdLineArgs.clear();
		for (size_t i = 0, iSz = NextGameCmdLineArgs->size(); i < iSz; i++) {
			Application::CmdLineArgs.push_back((*NextGameCmdLineArgs)[i]);
		}

		delete NextGameCmdLineArgs;
		NextGameCmdLineArgs = nullptr;
	}

	Application::StartGame(startingScene);
	Application::UpdateWindowTitle();

	return true;
}

bool Application::SetNextGame(const char* path,
	const char* startingScene,
	std::vector<std::string>* cmdLineArgs) {
	bool exists = false;

	std::string resolved = "";
	PathLocation location;

	if (Path::FromURL(path, resolved, location, false) &&
		(location == PathLocation::GAME || location == PathLocation::USER)) {
		if (path[strlen(path) - 1] == '/') {
			exists = Directory::Exists(resolved.c_str());
		}
		else {
			exists = File::Exists(resolved.c_str());
		}
	}

	if (exists) {
		StringUtils::Copy(NextGame, resolved.c_str(), sizeof NextGame);

		if (startingScene != nullptr) {
			StringUtils::Copy(
				NextGameStartingScene, startingScene, sizeof NextGameStartingScene);
		}

		NextGameCmdLineArgs = cmdLineArgs;

		return true;
	}

	Log::Print(Log::LOG_ERROR, "Path \"%s\" is not valid!", path);

	return false;
}

void Application::LoadVideoSettings() {
	bool vsyncEnabled;
	Application::Settings->GetBool("display", "vsync", &vsyncEnabled);
	Application::Settings->GetInteger("display", "frameSkip", &Application::FrameSkip);
	Application::Settings->GetBool("graphics", "showFramerate", &Application::ShowFPS);

	if (Application::FrameSkip > DEFAULT_MAX_FRAMESKIP) {
		Application::FrameSkip = DEFAULT_MAX_FRAMESKIP;
	}

	if (Graphics::Initialized) {
		Graphics::SetVSync(vsyncEnabled);
	}
	else {
		Graphics::VsyncEnabled = vsyncEnabled;

		Application::Settings->GetInteger(
			"display", "defaultMonitor", &Application::DefaultMonitor);

		Application::Settings->GetInteger(
			"graphics", "multisample", &Graphics::MultisamplingEnabled);
		Application::Settings->GetBool(
			"graphics", "precompileShaders", &Graphics::PrecompileShaders);

		if (Graphics::MultisamplingEnabled < 0) {
			Graphics::MultisamplingEnabled = 0;
		}
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
	GET_KEY("devMenuToggle", DevMenuToggle, Key_ESCAPE);
	GET_KEY("toggleFPSCounter", ToggleFPSCounter, Key_F2);
	GET_KEY("devRestartApp", DevRestartApp, Key_F1);
	GET_KEY("devRestartScene", DevRestartScene, Key_F6);
	GET_KEY("devRecompile", DevRecompile, Key_F5);
	GET_KEY("devPerfSnapshot", DevPerfSnapshot, Key_F3);
	GET_KEY("devLogLayerInfo", DevLayerInfo, Key_F7);
	GET_KEY("devFastForward", DevFastForward, Key_BACKSPACE);
	GET_KEY("devToggleFrameStepper", DevFrameStepper, Key_F9);
	GET_KEY("devStepFrame", DevStepFrame, Key_F10);
	
	GET_KEY("devQuit", DevQuit, Key_UNKNOWN);
	GET_KEY("devShowTileCol", DevTileCol, Key_UNKNOWN);
	GET_KEY("devShowObjectRegions", DevObjectRegions, Key_UNKNOWN);

#undef GET_KEY
}

void Application::AddPerformanceMetric(PerformanceMeasure* measure,
	const char* name,
	float r,
	float g,
	float b) {
	*measure = PerformanceMeasure(name, r, g, b);
	AllMetrics.push_back(measure);
}
void Application::AddPerformanceMetric(PerformanceMeasure* measure,
	const char* name,
	float r,
	float g,
	float b,
	bool* isActive) {
	*measure = PerformanceMeasure(name, r, g, b, isActive);
	AllMetrics.push_back(measure);
}

void Application::InitPerformanceMetrics() {
	AutomaticPerformanceSnapshots = false;
	AutomaticPerformanceSnapshotFrameTimeThreshold = 20.0;
	AutomaticPerformanceSnapshotLastTime = 0.0;
	AutomaticPerformanceSnapshotMinInterval = 5000.0; // 5 seconds

	AllMetrics.clear();

	AddPerformanceMetric(&Metrics.Event, "Event Polling", 1.0, 0.0, 0.0);
	AddPerformanceMetric(&Metrics.AfterScene, "Post-Scene", 0.0, 1.0, 0.0);
	AddPerformanceMetric(&Metrics.Poll, "Input Polling", 0.0, 0.0, 1.0);
	AddPerformanceMetric(&Metrics.Update, "Entity Update", 1.0, 1.0, 0.0);
	AddPerformanceMetric(&Metrics.Clear, "Clear Time", 0.0, 1.0, 1.0);
	AddPerformanceMetric(&Metrics.Render, "World Render Commands", 1.0, 0.0, 1.0);
	AddPerformanceMetric(&Metrics.PostProcess,
		"Render Post-Process",
		1.0,
		1.0,
		1.0,
		&Graphics::UsingPostProcessShader);
	AddPerformanceMetric(&Metrics.Present, "Frame Present Time", 0.75, 0.75, 0.75);
}

void Application::LoadDevSettings() {
#ifdef DEVELOPER_MODE
	Application::Settings->GetBool("dev", "devMenu", &DevMode);
	Application::Settings->GetBool("dev", "viewPerformance", &ViewPerformance);
	Application::Settings->GetBool("dev", "doNothing", &DoNothing);
	Application::Settings->GetInteger("dev", "fastForward", &UpdatesPerFastForward);
	Application::Settings->GetBool("dev", "convertModels", &Application::DevConvertModels);
	Application::Settings->GetBool("dev", "useMemoryFileCache", &UseMemoryFileCache);
	Application::Settings->GetBool("dev", "loadAllClasses", &ScriptManager::LoadAllClasses);

	if (!Running) {
		Application::Settings->GetBool("dev", "writeLogFile", &Log::WriteToFile);
		Application::Settings->GetBool("dev", "trackMemory", &Memory::IsTracking);
	}

	int logLevel = 0;
#ifdef DEBUG
	logLevel = -1;
#endif

	bool hasLogLevelSetting = Application::Settings->GetInteger("dev", "logLevel", &logLevel);
	if (!Running || hasLogLevelSetting) {
		Log::SetLogLevel(logLevel);
	}

	Application::Settings->GetBool("dev", "autoPerfSnapshots", &AutomaticPerformanceSnapshots);
	int apsFrameTimeThreshold = 0, apsMinInterval = 0;
	if (Application::Settings->GetInteger("dev", "apsMinFrameTime", &apsFrameTimeThreshold)) {
		AutomaticPerformanceSnapshotFrameTimeThreshold = apsFrameTimeThreshold;
	}
	if (Application::Settings->GetInteger("dev", "apsMinInterval", &apsMinInterval)) {
		AutomaticPerformanceSnapshotMinInterval = apsMinInterval;
	}

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
	if (Application::DevMenuActivated) {
		DevMenu.CurrentWindowWidth = window_w;
		DevMenu.CurrentWindowHeight = window_h;
	}

	Graphics::Resize(window_w, window_h);
}

bool Application::GetWindowFullscreen() {
	return !!(SDL_GetWindowFlags(Application::Window) & SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void Application::SetWindowFullscreen(bool isFullscreen) {
	SDL_SetWindowFullscreen(Application::Window, isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	Application::WindowFullscreen = isFullscreen;
	Application::Settings->SetBool("display", "fullscreen", Application::WindowFullscreen);

	int window_w, window_h;
	SDL_GetWindowSize(Application::Window, &window_w, &window_h);

	if (Application::DevMenuActivated) {
		DevMenu.Fullscreen = isFullscreen;
		DevMenu.CurrentWindowWidth = window_w;
		DevMenu.CurrentWindowHeight = window_h;
	}

	Graphics::Resize(window_w, window_h);
}

void Application::SetWindowBorderless(bool isBorderless) {
	Application::WindowBorderless = isBorderless;
	Application::Settings->SetBool("display", "borderless", isBorderless);
	SDL_SetWindowBordered(Application::Window, (SDL_bool)(!isBorderless));
}

void Application::SetWindowScale(int scale) {
	if (scale < 0)
		scale = 0;
	if (scale > 8)
		scale = 8;
	Application::WindowScale = scale;
	Application::Settings->SetInteger("display", "scale", Application::WindowScale);
	Application::SetWindowSize(Application::WindowWidth * Application::WindowScale, Application::WindowHeight * Application::WindowScale);
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
				Application::SetWindowFullscreen(!Application::GetWindowFullscreen());
				break;
			}
			// Toggle FPS counter
			else if (key == KeyBindsSDL[(int)KeyBind::ToggleFPSCounter]) {
				Application::ShowFPS = !Application::ShowFPS;
				break;
			}

			if (DevMode) {
				// Quit application (dev)
				// Unbound by default, must be set in GameConfig
				if (key == KeyBindsSDL[(int)KeyBind::DevQuit]) {
					Running = false;
					break;
				}
				// Open dev menu (dev)
				if (key == KeyBindsSDL[(int)KeyBind::DevMenuToggle]) {
					Application::DevMenuActivated ? Application::CloseDevMenu() : Application::OpenDevMenu();
					break;
				}
				// Restart application (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevRestartApp]) {
					Application::Restart(false);
					Application::StartGame(StartingScene);
					if (Application::DevMenuActivated)
						Application::CloseDevMenu();
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
					char lastScene[MAX_RESOURCE_PATH_LENGTH];
					memcpy(lastScene,
						Scene::CurrentScene,
						MAX_RESOURCE_PATH_LENGTH);

					Application::Restart(true);
					Application::StartGame(lastScene);
					if (Application::DevMenuActivated)
						Application::CloseDevMenu();
					Application::UpdateWindowTitle();
					break;
				}
				// Restart scene (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevRestartScene]) {
					// Reset FPS timer
					BenchmarkFrameCount = 0;

					InputManager::ControllerStopRumble();

					if (Application::DevMenuActivated)
						Application::CloseDevMenu();

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
					break;
				}
				// Toggle frame stepper (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevFrameStepper]) {
					Stepper = !Stepper;
					Application::UpdateWindowTitle();
					break;
				}
				// Step frame (dev)
				else if (key == KeyBindsSDL[(int)KeyBind::DevStepFrame]) {
					Stepper = true;
					Step = true;
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
	Metrics.Frame.Begin();

	FrameTimeStart = Clock::GetTicks();

	// Event loop
	Metrics.Event.Begin();
	Application::PollEvents();
	Metrics.Event.End();

	// BUG: Having Stepper on prevents the first
	//   frame of a new scene from Updating, but still rendering.
	if (*Scene::NextScene) {
		Step = true;
	}

	FirstFrame = false;

	Metrics.AfterScene.Begin();
	Scene::AfterScene();
	Metrics.AfterScene.End();

	if (DoNothing) {
		goto DO_NOTHING;
	}

	// Update
	for (int m = 0; m < runFrames; m++) {
		Scene::ResetPerf();
		Metrics.Poll.Reset();
		Metrics.Update.Reset();
		if (((Stepper && Step) || !Stepper) && !Application::DevMenuActivated) {
			// Poll for inputs
			Metrics.Poll.Begin();
			InputManager::Poll();
			Metrics.Poll.Accumulate();

			// Update scene
			Metrics.Update.Begin();
			Scene::Update();
			Metrics.Update.Accumulate();
		}
		Step = false;
		if (runFrames != 1 && (*Scene::NextScene || Scene::DoRestart)) {
			break;
		}
	}

#ifdef USING_FFMPEG
	AudioManager::Lock();
	Uint8 audio_buffer[0x8000]; // <-- Should be larger than AudioManager::AudioQueueMaxSize
	int needed = 0x8000; // AudioManager::AudioQueueMaxSize;
	for (size_t i = 0, i_sz = Scene::MediaList.size(); i < i_sz; i++) {
		if (!Scene::MediaList[i]) {
			continue;
		}

		MediaBag* media = Scene::MediaList[i]->AsMedia;
		int queued = (int)AudioManager::AudioQueueSize;
		if (queued < needed) {
			int ready_bytes =
				media->Player->GetAudioData(audio_buffer, needed - queued);
			if (ready_bytes > 0) {
				memcpy(AudioManager::AudioQueue + AudioManager::AudioQueueSize,
					audio_buffer,
					ready_bytes);
				AudioManager::AudioQueueSize += ready_bytes;
			}
		}
	}
	AudioManager::Unlock();
#endif

	// Rendering
	Metrics.Clear.Begin();
	Graphics::Clear();
	Metrics.Clear.End();

	Metrics.Render.Begin();
	Scene::Render();
	Metrics.Render.End();

	if (Graphics::UsingPostProcessShader) {
		Metrics.PostProcess.Begin();
		Graphics::DoScreenPostProcess();
		Metrics.PostProcess.End();
	}

DO_NOTHING:
	RunDevMenu();

	// Show FPS counter
	Metrics.FPSCounter.Begin();
	DrawPerformance();
	Metrics.FPSCounter.End();

	Metrics.Present.Begin();
	Graphics::Present();
	Metrics.Present.End();

	Metrics.Frame.End();
}
void Application::DrawPerformance() {
	if (DefaultFont == nullptr) {
		return;
	}

	if (ViewPerformance) {
		PerformanceViewer::DrawDetailed(DefaultFont);
	}
	else if (Application::ShowFPS) {
		PerformanceViewer::DrawFramerate(DefaultFont);
	}
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
void Application::StartGame(const char* startingScene) {
	Application::LoadDefaultFont();
	Application::InitScripting();

	Scene::Init();
	Scene::Prepare();
	Scene::Initialize();

	// Load initial scripts
	ScriptManager::LoadScript("init.hsl");

	if (ScriptManager::LoadAllClasses) {
		ScriptManager::LoadClasses();
	}

	// Load Static class
	Scene::AddStaticClass();

	// Don't prepare the scene twice if there is no scene to load.
	if (startingScene[0] != '\0') {
		Scene::LoadScene(startingScene);
	}

	// Call Static's GameStart here
	Scene::CallGameStart();

	// Start scene
	if (Scene::CurrentScene[0] != '\0') {
		Scene::Restart();
	}
	else {
		Scene::FinishLoad();
	}
}
void Application::Run(int argc, char* args[]) {
	Application::Init(argc, args);
	if (!Running) {
		return;
	}

	char scenePath[MAX_RESOURCE_PATH_LENGTH];
	StringUtils::Copy(scenePath, StartingScene, sizeof scenePath);

	// Run scene from command line argument
	if (argc > 1 && AllowCmdLineSceneLoad) {
		char* pathStart = StringUtils::StrCaseStr(args[1], "/Resources/");
		if (pathStart == NULL) {
			pathStart = StringUtils::StrCaseStr(args[1], "\\Resources\\");
		}

		if (pathStart) {
			StringUtils::Copy(
				scenePath, pathStart + strlen("/Resources/"), sizeof scenePath);
			StringUtils::ReplacePathSeparatorsInPlace(scenePath);
		}
		else {
			Log::Print(Log::LOG_WARN,
				"Map file \"%s\" not inside Resources folder!",
				args[1]);
		}
	}

	Application::StartGame(scenePath);
	Application::UpdateWindowTitle();
	Application::SetWindowSize(Application::WindowWidth * Application::WindowScale, Application::WindowHeight * Application::WindowScale);

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
			int lagFrames = ((int)round(timeTaken / FrameTimeDesired)) - 1;
			if (lagFrames > Application::FrameSkip) {
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
			Metrics.Frame.Time > AutomaticPerformanceSnapshotFrameTimeThreshold) {
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

		if (NextGame[0] != '\0') {
			char gamePath[MAX_PATH_LENGTH];
			StringUtils::Copy(gamePath, NextGame, sizeof gamePath);
			NextGame[0] = '\0';

			ChangeGame(gamePath);
		}
	}

	Scene::Dispose();

	Application::Cleanup();
#endif
}

void Application::Cleanup() {
	Application::TerminateScripting();

	Application::UnloadDefaultFont();
	Application::DefaultFontList.clear();

	Application::DisposeSettings();

	MemoryCache::Dispose();
	ResourceManager::Dispose();
	AudioManager::Dispose();
	InputManager::Dispose();

	Graphics::Dispose();

	Application::CmdLineArgs.clear();

	if (NextGameCmdLineArgs) {
		delete NextGameCmdLineArgs;
		NextGameCmdLineArgs = nullptr;
	}

	for (std::unordered_map<std::string, Capability>::iterator it = CapabilityMap.begin();
		it != CapabilityMap.end();
		it++) {
		it->second.Dispose();
	}
	CapabilityMap.clear();

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
void Application::TerminateScripting() {
	ScriptManager::Dispose();
	SourceFileMap::Dispose();
	Compiler::Dispose();
	GarbageCollector::Dispose();

	ScriptManager::LoadAllClasses = false;
	ScriptEntity::DisableAutoAnimate = false;
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

	if (!Running) {
		LogFilename[0] = '\0';
	}

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

		ParseGameConfigBool(node, "allowCmdLineSceneLoad", Application::AllowCmdLineSceneLoad);
		ParseGameConfigBool(node, "disableDefaultActions", Application::DisableDefaultActions);
		ParseGameConfigBool(node, "loadAllClasses", ScriptManager::LoadAllClasses);
		ParseGameConfigBool(node, "useSoftwareRenderer", Graphics::UseSoftwareRenderer);
		ParseGameConfigBool(node, "enablePaletteUsage", Graphics::UsePalettes);
		ParseGameConfigText(node, "settingsFile", SettingsFile, sizeof SettingsFile);

		if (!Running) {
			ParseGameConfigBool(node, "writeLogFile", Log::WriteToFile);
			ParseGameConfigText(node, "logFilename", LogFilename, sizeof LogFilename);
		}
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

void Application::InitGameInfo() {
	StringUtils::Copy(GameTitle, DEFAULT_GAME_TITLE, sizeof(GameTitle));
	StringUtils::Copy(GameTitleShort, DEFAULT_GAME_SHORT_TITLE, sizeof(GameTitleShort));
	StringUtils::Copy(GameVersion, DEFAULT_GAME_VERSION, sizeof(GameVersion));
	StringUtils::Copy(GameDescription, DEFAULT_GAME_DESCRIPTION, sizeof(GameDescription));

	StringUtils::Copy(GameIdentifier, DEFAULT_GAME_IDENTIFIER, sizeof(GameIdentifier));
	StringUtils::Copy(SavesDir, DEFAULT_SAVES_DIR, sizeof(SavesDir));
}

void Application::LoadGameInfo() {
	bool shouldSetGameId = false;
	bool shouldSetGameDevId = false;

	if (GameConfig) {
		XMLNode* root = GameConfig->children[0];

		XMLNode* node = XMLParser::SearchNode(root, "gameTitle");
		if (node == nullptr) {
			node = XMLParser::SearchNode(root, "name");
		}

		if (node) {
			XMLParser::CopyTokenToString(
				node->children[0]->name, GameTitle, sizeof(GameTitle));
			StringUtils::Copy(GameTitleShort, GameTitle, sizeof(GameTitleShort));
			shouldSetGameId = true;
		}

		node = XMLParser::SearchNode(root, "shortTitle");
		if (node) {
			XMLParser::CopyTokenToString(
				node->children[0]->name, GameTitleShort, sizeof(GameTitleShort));
			shouldSetGameId = true;
		}

		node = XMLParser::SearchNode(root, "version");
		if (node) {
			std::string versionText = ParseGameVersion(node);
			if (versionText.size() > 0) {
				StringUtils::Copy(
					GameVersion, versionText.c_str(), sizeof(GameVersion));
			}
		}

		node = XMLParser::SearchNode(root, "description");
		if (node) {
			XMLParser::CopyTokenToString(
				node->children[0]->name, GameDescription, sizeof(GameDescription));
		}

		node = XMLParser::SearchNode(root, "developer");
		if (node) {
			XMLParser::CopyTokenToString(
				node->children[0]->name, GameDeveloper, sizeof(GameDeveloper));
			shouldSetGameDevId = true;
		}

		node = XMLParser::SearchNode(root, "gameIdentifier");
		if (node) {
			char* id = XMLParser::TokenToString(node->children[0]->name);
			if (ValidateAndSetIdentifier("game identifier",
				    id,
				    GameIdentifier,
				    sizeof(GameIdentifier))) {
				shouldSetGameId = false;
			}

			Memory::Free(id);
		}

		node = XMLParser::SearchNode(root, "developerIdentifier");
		if (node) {
			char* id = XMLParser::TokenToString(node->children[0]->name);
			if (ValidateAndSetIdentifier("developer identifier",
				    id,
				    DeveloperIdentifier,
				    sizeof(DeveloperIdentifier))) {
				shouldSetGameDevId = false;
			}

			Memory::Free(id);
		}

		ParseGameConfigBool(root, "useDeveloperIdentifierInPaths", shouldSetGameDevId);

		node = XMLParser::SearchNode(root, "savesDir");
		if (node) {
			char* id = XMLParser::TokenToString(node->children[0]->name);
			ValidateAndSetIdentifier("saves directory", id, SavesDir, sizeof(SavesDir));
			Memory::Free(id);
		}

		node = XMLParser::SearchNode(root, "preferencesDir");
		if (node) {
			char* id = XMLParser::TokenToString(node->children[0]->name);
			ValidateAndSetIdentifier("preferences directory",
				id,
				PreferencesDir,
				sizeof(PreferencesDir));
			Memory::Free(id);
		}
	}

	if (shouldSetGameId) {
		char* id = Application::GenerateIdentifier(GameTitleShort);
		if (id != nullptr) {
			StringUtils::Copy(GameIdentifier, id, sizeof(GameIdentifier));
			Memory::Free(id);
		}
	}

	if (shouldSetGameDevId) {
		char* id = Application::GenerateIdentifier(GameDeveloper);
		if (id != nullptr) {
			StringUtils::Copy(DeveloperIdentifier, id, sizeof(DeveloperIdentifier));
			Memory::Free(id);
		}
	}
}

void Application::LoadSceneInfo(int activeCategory, int currentSceneNum, bool keepScene) {
	XMLNode* sceneConfig = nullptr;
	int startSceneNum = currentSceneNum;

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
			// Read category and starting scene number to be used by the SceneConfig
			if (Application::GameConfig) {
				XMLNode* node = Application::GameConfig->children[0];
				if (node) {
					// Parse active category
					if (!ParseGameConfigInt(node, "activeCategory", Scene::ActiveCategory)) { // backwards compat
						char* text = ParseGameConfigText(node, "activeCategory");
						if (text) {
							int id = SceneInfo::GetCategoryID(text);
							if (id >= 0) {
								Scene::ActiveCategory = id;
							}
							Memory::Free(text);
						}
					}

					// Parse starting scene
					ParseGameConfigInt(node, "startSceneNum", startSceneNum); // backwards compat

					char* text = ParseGameConfigText(node, "startscene");
					if (text) {
						int id = SceneInfo::GetEntryID(Scene::ActiveCategory, text);
						if (id >= 0) {
							startSceneNum = id;
						}
						Memory::Free(text);
					}
				}
			}

			if (keepScene) {
				Scene::ActiveCategory = activeCategory;
				startSceneNum = currentSceneNum;
			}

			if (SceneInfo::IsEntryValid(Scene::ActiveCategory, startSceneNum)) {
				Scene::CurrentSceneInList = startSceneNum;
			}

			if (StartingScene[0] == '\0' && SceneInfo::CategoryHasEntries(Scene::ActiveCategory)) {
				Scene::SetInfoFromCurrentID();

				StringUtils::Copy(StartingScene,SceneInfo::GetFilename(Scene::ActiveCategory, Scene::CurrentSceneInList).c_str(), sizeof(StartingScene));
			}

			Log::Print(Log::LOG_VERBOSE, "Loaded scene list (%d categories, %d scenes)", SceneInfo::Categories.size(), SceneInfo::NumTotalScenes);
		}

		XMLParser::Free(sceneConfig);
	}
}

void Application::InitPlayerControls() {
	InputManager::InitPlayerControls();

	if (Application::DisableDefaultActions)
		return;

	std::unordered_map<std::string, unsigned> actions;

	const char* name[] = {"Up", "Down", "Left", "Right", "A", "B", "C", "X", "Y", "Z", "Start", "Select"};
	const int keys[] = { Key_UP, Key_DOWN, Key_LEFT, Key_RIGHT, Key_A, Key_S, Key_D, Key_Q, Key_W, Key_E, Key_RETURN, Key_TAB };
	const int buttons[] = { (int)ControllerButton::DPadUp, (int)ControllerButton::DPadDown, (int)ControllerButton::DPadLeft, (int)ControllerButton::DPadRight,
		(int)ControllerButton::A, (int)ControllerButton::B, (int)ControllerButton::LeftShoulder,
		(int)ControllerButton::X, (int)ControllerButton::Y, (int)ControllerButton::RightShoulder,
		(int)ControllerButton::Start, (int)ControllerButton::Back };

	for (size_t i = 0; i < std::size(name); ++i) {
		actions[name[i]] = InputManager::RegisterAction(name[i]);
	}

	if (InputManager::Players.size() < 1) {
		InputManager::AddPlayer();
		InputManager::SetPlayerControllerIndex(0, 0);
	}

	for (size_t p = 0; p < InputManager::Players.size(); ++p) {
		InputPlayer& player = InputManager::Players[p];

		for (size_t b = 0; b < std::size(name); ++b) {
			KeyboardBind* keyboardBind = new KeyboardBind(keys[b]);
			ControllerButtonBind* controllerBind = new ControllerButtonBind(buttons[b]);

			if (!player.GetDefaultBind(actions[name[b]], p)) {
				if (player.AddDefaultBind(actions[name[b]], keyboardBind) < 0) {
					delete keyboardBind;
				}

				if (player.AddDefaultBind(actions[name[b]], controllerBind) < 0) {
					delete controllerBind;
				}

				if (strcmp(name[b], "Up") == 0 ||
					strcmp(name[b], "Down") == 0 ||
					strcmp(name[b], "Left") == 0 ||
					strcmp(name[b], "Right") == 0)
				{
					ControllerAxisBind* axisBind = new ControllerAxisBind();
					axisBind->AxisDeadzone = 0.0;
					axisBind->AxisDigitalThreshold = DEFAULT_DIGITAL_AXIS_THRESHOLD;

					if (strcmp(name[b], "Up") == 0) {
						axisBind->IsAxisNegative = true;
						axisBind->Axis = (int)ControllerAxis::LeftY;
					}
					else if (strcmp(name[b], "Down") == 0) {
						axisBind->IsAxisNegative = false;
						axisBind->Axis = (int)ControllerAxis::LeftY;
					}
					else if (strcmp(name[b], "Left") == 0) {
						axisBind->IsAxisNegative = true;
						axisBind->Axis = (int)ControllerAxis::LeftX;
					}
					else if (strcmp(name[b], "Right") == 0) {
						axisBind->IsAxisNegative = false;
						axisBind->Axis = (int)ControllerAxis::LeftX;
					}

					if (player.AddDefaultBind(actions[name[b]], axisBind) < 0) {
						delete axisBind;
					}
				}
			}
		}

		player.ResetBinds();
	}
}

bool Application::LoadSettings(const char* filename) {
	INI* ini = INI::Load(filename);
	if (ini == nullptr) {
		return false;
	}

	if (filename != Application::SettingsFile) {
		StringUtils::Copy(
			Application::SettingsFile, filename, sizeof(Application::SettingsFile));
	}

	Application::DisposeSettings();
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
		if (Application::SettingsFile[0] == '\0') {
			StringUtils::Copy(Application::SettingsFile,
				DEFAULT_SETTINGS_FILENAME,
				sizeof(Application::SettingsFile));
		}

		Application::InitSettings();
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

void Application::InitSettings() {
	// Create settings with default values.
	Application::Settings = INI::New(Application::SettingsFile);

	Application::Settings->SetBool("display", "fullscreen", false);
	Application::Settings->SetInteger("display", "scale", 2);
	Application::Settings->SetBool("display", "vsync", false);
	Application::Settings->SetBool("display", "borderless", false);
	Application::Settings->SetInteger("display", "defaultMonitor", 0);
	Application::Settings->SetBool("display", "retina", false);
	Application::Settings->SetInteger("display", "multisample", 0);
	Application::Settings->SetInteger("display", "frameSkip", DEFAULT_MAX_FRAMESKIP);

#ifdef DEVELOPER_MODE
	Application::Settings->SetBool("dev", "devMenu", true);
	Application::Settings->SetBool("dev", "writeLogFile", true);
	Application::Settings->SetBool("dev", "trackMemory", false);
	Application::Settings->SetBool("dev", "viewPerformance", false);
	Application::Settings->SetInteger("dev", "fastForward", 4);
	int logLevel = 0;
#ifdef DEBUG
	logLevel = -1;
#endif
	Application::Settings->SetInteger("dev", "logLevel", logLevel);
	Application::Settings->SetBool("dev", "trackMemory", false);
	Application::Settings->SetBool("dev", "autoPerfSnapshots", false);
#endif
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

void Application::DisposeSettings() {
	if (Application::Settings) {
		Application::Settings->Dispose();
		Application::Settings = nullptr;
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

void Application::DrawDevString(const char* string, int x, int y, int align, bool isSelected) {
	TextDrawParams textParams;

	textParams.FontSize = 15 * (DevMenu.CurrentWindowWidth / Application::WindowWidth);
	textParams.Ascent = DefaultFont->Ascent;
	textParams.Descent = DefaultFont->Descent;
	textParams.Leading = DefaultFont->Leading;

	float maxW = 0.0, maxH = 0.0;
	Graphics::MeasureText(DefaultFont, string, &textParams, maxW, maxH);
	if (align == ALIGN_CENTER) x -= (maxW / 2) / (DevMenu.CurrentWindowWidth / Application::WindowWidth);
	else if (align == ALIGN_RIGHT) x -= maxW / (DevMenu.CurrentWindowWidth / Application::WindowWidth);

	Graphics::SetBlendColor(0.28125, 0.28125, 0.4375, 1.0);
	// Graphics::DrawText(DefaultFont, string, (x + 0.5) * (DevMenu.CurrentWindowWidth / Application::WindowWidth), (y + 0.5) * (DevMenu.CurrentWindowHeight / Application::WindowHeight), &textParams);
	isSelected ? Graphics::SetBlendColor(0.9375, 0.9375, 0.9375, 1.0) : Graphics::SetBlendColor(0.5, 0.625, 0.6875, 1.0);
	Graphics::DrawText(DefaultFont, string, x * (DevMenu.CurrentWindowWidth / Application::WindowWidth), y * (DevMenu.CurrentWindowHeight / Application::WindowHeight), &textParams);
}

void Application::RunDevMenu() {
	if (DefaultFont == nullptr) {
		Application::CloseDevMenu();
		return;
	}

	SDL_GetWindowSize(Application::Window, &DevMenu.CurrentWindowWidth, &DevMenu.CurrentWindowHeight);
	Graphics::UpdateOrthoFlipped(DevMenu.CurrentWindowWidth, DevMenu.CurrentWindowHeight);
	Graphics::SetBlendMode(BlendFactor_SRC_ALPHA,
		BlendFactor_INV_SRC_ALPHA,
		BlendFactor_ONE,
		BlendFactor_INV_SRC_ALPHA);

	Graphics::Save();
	if (DevMenuActivated) {
		// Poll for inputs, since the frame did not run
		InputManager::Poll();

		if (Application::DevMenu.State)
			Application::DevMenu.State();
	}
	Graphics::Restore();
}

void Application::OpenDevMenu() {
	if (Application::DisableDefaultActions)
		return;

	DevMenu.State = Application::DevMenu_MainMenu;
	DevMenu.Selection = 0;
	DevMenu.ScrollPos = 0;
	DevMenu.SubSelection = 0;
	DevMenu.SubScrollPos = 0;
	DevMenu.Timer = 0;

	DevMenu.WindowScale = Application::WindowScale;
	SDL_GetWindowSize(Application::Window, &DevMenu.CurrentWindowWidth, &DevMenu.CurrentWindowHeight);
	DevMenu.Fullscreen = Application::WindowFullscreen;
	DevMenu.WindowBorderless = Application::WindowBorderless;

	AudioManager::AudioPauseAll();
	AudioManager::Lock();
	if (AudioManager::MusicStack.size() > 0) {
		DevMenu.MusicPausedStore = AudioManager::MusicStack[0]->Paused;
		AudioManager::MusicStack[0]->Paused = true;
	}
	AudioManager::Unlock();

	Application::DevMenuActivated = true;
}

void Application::CloseDevMenu() {
	Application::DevMenuActivated = false;

	DevMenu.WindowScale = Application::WindowScale;
	DevMenu.Fullscreen = Application::WindowFullscreen;
	DevMenu.WindowBorderless = Application::WindowBorderless;

	Application::SaveSettings();
	Application::LoadAudioSettings();

	AudioManager::AudioUnpauseAll();
	AudioManager::Lock();
	if (AudioManager::MusicStack.size() > 0) {
		AudioManager::MusicStack[0]->Paused = DevMenu.MusicPausedStore;
		DevMenu.MusicPausedStore = false;
	}
	AudioManager::Unlock();
}

void Application::SetBlendColor(int color) {
	Graphics::SetBlendColor(
		(color >> 16 & 0xFF) / 255.f,
		(color >> 8 & 0xFF) / 255.f,
		(color & 0xFF) / 255.f, 1.0);
}

void Application::DrawRectangle(float x, float y, float width, float height, int color, int alpha) {
	Graphics::SetBlendColor(
		(color >> 16 & 0xFF) / 255.f,
		(color >> 8 & 0xFF) / 255.f,
		(color & 0xFF) / 255.f, alpha / 256.0f);
	Graphics::FillRectangle(x * (DevMenu.CurrentWindowWidth / Application::WindowWidth), y * (DevMenu.CurrentWindowHeight / Application::WindowHeight), width * (DevMenu.CurrentWindowWidth / Application::WindowWidth), height * (DevMenu.CurrentWindowHeight / Application::WindowHeight));
}

void Application::DevMenu_DrawMainMenu() {
	const char* selectionNames[] = { "Resume", "Restart", "Scene Select", "Settings", "Exit" };

	DevMenu_DrawTitleBar();
	DrawRectangle(0, 82, 128, 124, 0x000000, 0xC0);
	DrawRectangle(144, 82, Application::WindowWidth - 144, 124, 0x000000, 0xC0);

	for (size_t i = 0, y = 109; i < std::size(selectionNames); ++i, y += 14)
		DrawDevString(selectionNames[i], 16, y, ALIGN_LEFT, DevMenu.Selection == i);
}

void Application::DevMenu_DrawTitleBar() {
	DrawRectangle(0, 16, Application::WindowWidth, 54, 0x000000, 0xC0);
	DrawDevString("Hatch Engine Developer Menu", Application::WindowWidth / 2, 20, ALIGN_CENTER, true);
	DrawDevString(GameTitleShort, Application::WindowWidth / 2, 35, ALIGN_CENTER, true);
}

void Application::DevMenu_MainMenu() {
	DevMenu_DrawMainMenu();

	char versionDisplay[544];
	snprintf(versionDisplay, sizeof(versionDisplay), "Engine version %s, game version %s", Application::EngineVersion, Application::GameVersion);
	DrawDevString(versionDisplay, Application::WindowWidth / 2, 50, ALIGN_CENTER, true);

	const char* tooltips[] = {
		"Resume the game.", "Restart the current scene.", "Navigate to a certain scene.",
		"Adjust the application's settings.", "Close the application."
	};
	DrawDevString(tooltips[DevMenu.Selection], 160, 86, ALIGN_LEFT, true);

	int actionUp = InputManager::GetActionID("Up");
	int actionDown = InputManager::GetActionID("Down");

	if ((actionUp != -1 && (InputManager::IsActionPressedByAny(actionUp) || ((actionUp != -1 && InputManager::IsActionHeldByAny(actionUp)) && !DevMenu.Timer))) ||
		(actionDown != -1 && (InputManager::IsActionPressedByAny(actionDown) || ((actionDown != -1 && InputManager::IsActionHeldByAny(actionDown)) && !DevMenu.Timer)))) {

		DevMenu.Selection = (DevMenu.Selection + (((actionUp != -1 && InputManager::IsActionPressedByAny(actionUp)) || (actionUp != -1 && InputManager::IsActionHeldByAny(actionUp))) ? -1 : 1) + (int)std::size(tooltips)) % (int)std::size(tooltips);
		DevMenu.Timer = 8;
	}

	if (DevMenu.Timer > 0) DevMenu.Timer = ++DevMenu.Timer & 7;

	bool confirm = false;
	for (const char* action : { "A", "Start" }) {
		int actionID = InputManager::GetActionID(action);
		if (actionID != -1 && InputManager::IsActionPressedByAny(actionID)) {
			confirm = true;
			break;
		}
	}

	if (confirm) {
		switch (DevMenu.Selection) {
			case 0: CloseDevMenu(); break;
			case 1:
				CloseDevMenu();
				BenchmarkFrameCount = 0;
				InputManager::ControllerStopRumble();
				Scene::Restart();
				UpdateWindowTitle();
				break;
			case 2: DevMenu.State = Application::DevMenu_CategorySelectMenu; break;
			case 3: DevMenu.State = Application::DevMenu_SettingsMenu; break;
			case 4: Running = false; break;
		}
		DevMenu.SubSelection = 0;
		DevMenu.Timer = 1;
	}
	else {
		int actionB = InputManager::GetActionID("B");
		if (actionB != -1 && InputManager::IsActionPressedByAny(actionB)) {
			CloseDevMenu();
		}
	}
}

void Application::DevMenu_CategorySelectMenu() {
	DevMenu_DrawMainMenu();

	if (!ResourceManager::ResourceExists("Game/SceneConfig.xml")) {
		DrawDevString("No SceneConfig is loaded!", 160, 86, ALIGN_LEFT, true);

		int actionB = InputManager::GetActionID("B");
		if (actionB != -1 && InputManager::IsActionPressedByAny(actionB)) {
			DevMenu.State = DevMenu_MainMenu;
			DevMenu.SubSelection = 0;
			DevMenu.Timer = 1;
		}
		return;
	}

	DrawDevString("Select Scene Category...", Application::WindowWidth / 2, 50, ALIGN_CENTER, true);

	for (size_t i = 0, y = 86; i < 8 && DevMenu.SubScrollPos + i < SceneInfo::Categories.size(); i++, y += 14)
		DrawDevString(SceneInfo::Categories[DevMenu.SubScrollPos + i].Name, 160, y, ALIGN_LEFT, (DevMenu.SubSelection - DevMenu.SubScrollPos) == i);

	int actionUp = InputManager::GetActionID("Up");
	int actionDown = InputManager::GetActionID("Down");

	if ((actionUp != -1 && (InputManager::IsActionPressedByAny(actionUp) || (InputManager::IsActionHeldByAny(actionUp) && !DevMenu.Timer))) ||
		(actionDown != -1 && (InputManager::IsActionPressedByAny(actionDown) || (InputManager::IsActionHeldByAny(actionDown) && !DevMenu.Timer)))) {

		DevMenu.SubSelection = (DevMenu.SubSelection + (((actionUp != -1 && InputManager::IsActionPressedByAny(actionUp)) || (actionUp != -1 && InputManager::IsActionHeldByAny(actionUp))) ? -1 : 1) + (int)SceneInfo::Categories.size()) % (int)SceneInfo::Categories.size();

		if (DevMenu.SubSelection >= DevMenu.SubScrollPos) {
			if (DevMenu.SubSelection > DevMenu.SubScrollPos + 7)
				DevMenu.SubScrollPos = DevMenu.SubSelection - 7;
		}
		else {
			DevMenu.SubScrollPos = DevMenu.SubSelection;
		}

		DevMenu.Timer = 8;
	}

	if (DevMenu.Timer > 0) DevMenu.Timer = ++DevMenu.Timer & 7;

	bool confirm = false;
	for (const char* action : { "A", "Start" }) {
		int actionID = InputManager::GetActionID(action);
		if (actionID != -1 && InputManager::IsActionPressedByAny(actionID)) {
			confirm = true;
			break;
		}
	}

	if (confirm) {
		SceneListCategory* list = &SceneInfo::Categories[DevMenu.SubSelection];
		if (!list->Entries.empty()) {
			DevMenu.State = DevMenu_SceneSelectMenu;
			DevMenu.ListPos = DevMenu.SubSelection;
			DevMenu.SubScrollPos = DevMenu.SubSelection = 0;
		}
	}
	else {
		int actionB = InputManager::GetActionID("B");
		if (actionB != -1 && InputManager::IsActionPressedByAny(actionB)) {
			DevMenu.State = DevMenu_MainMenu;
			DevMenu.SubSelection = 0;
			DevMenu.Timer = 1;
		}
	}
}

void Application::DevMenu_SceneSelectMenu() {
	DevMenu_DrawMainMenu();

	DrawDevString("Select Scene...", Application::WindowWidth / 2, 50, ALIGN_CENTER, true);

	SceneListCategory* list = &SceneInfo::Categories[DevMenu.ListPos];

	for (size_t i = 0, y = 86; i < 8 && DevMenu.SubScrollPos + i < list->Entries.size(); i++, y += 14)
		DrawDevString(list->Entries[DevMenu.SubScrollPos + i].Name, 160, y, ALIGN_LEFT, DevMenu.SubSelection - DevMenu.SubScrollPos == i);

	int actionUp = InputManager::GetActionID("Up");
	int actionDown = InputManager::GetActionID("Down");

	if ((actionUp != -1 && (InputManager::IsActionPressedByAny(actionUp) || (InputManager::IsActionHeldByAny(actionUp) && !DevMenu.Timer))) ||
		(actionDown != -1 && (InputManager::IsActionPressedByAny(actionDown) || (InputManager::IsActionHeldByAny(actionDown) && !DevMenu.Timer)))) {

		DevMenu.SubSelection = (DevMenu.SubSelection + (((actionUp != -1 && InputManager::IsActionPressedByAny(actionUp)) || (actionUp != -1 && InputManager::IsActionHeldByAny(actionUp))) ? -1 : 1) + (int)list->Entries.size()) % (int)list->Entries.size();

		if (DevMenu.SubSelection >= DevMenu.SubScrollPos) {
			if (DevMenu.SubSelection > DevMenu.SubScrollPos + 7)
				DevMenu.SubScrollPos = DevMenu.SubSelection - 7;
		}
		else {
			DevMenu.SubScrollPos = DevMenu.SubSelection;
		}

		DevMenu.Timer = 8;
	}

	if (DevMenu.Timer > 0) DevMenu.Timer = ++DevMenu.Timer & 7;

	bool confirm = false;
	for (const char* action : { "A", "Start" }) {
		int actionID = InputManager::GetActionID(action);
		if (actionID != -1 && InputManager::IsActionPressedByAny(actionID)) {
			confirm = true;
			break;
		}
	}

	if (confirm) {
		CloseDevMenu();
		AudioManager::AudioStopAll();
		AudioManager::ClearMusic();

		const char* categoryName = list->Name;
		const char* sceneName = list->Entries[DevMenu.SubSelection].Name;

		int categoryID = SceneInfo::GetCategoryID(categoryName);
		int entryID = SceneInfo::GetEntryID(categoryName, sceneName);
		if (categoryID < 0 || entryID < 0) return;

		Scene::SetCurrent(categoryName, sceneName);
		StringUtils::Copy(Scene::NextScene, SceneInfo::GetFilename(categoryID, DevMenu.SubSelection).c_str(), sizeof(Scene::NextScene));
	}
	else {
		int actionB = InputManager::GetActionID("B");
		if (actionB != -1 && InputManager::IsActionPressedByAny(actionB)) {
			DevMenu.State = DevMenu_CategorySelectMenu;
			DevMenu.SubScrollPos = DevMenu.SubSelection = 0;
			DevMenu.ListPos = 1;
		}
	}
}

void Application::DevMenu_SettingsMenu() {
	DevMenu_DrawMainMenu();

	DrawDevString("Change settings...", Application::WindowWidth / 2, 50, ALIGN_CENTER, true);

	std::string labels[] = {
		"Video Settings",
		"Audio Settings",
		std::string("Performance Viewer ") + (ViewPerformance ? "(On)" : "(Off)"),
		std::string("Tile Collision Viewer (Path A) ") + ((Scene::ShowTileCollisionFlag == 1) ? "(On)" : "(Off)"),
		std::string("Tile Collision Viewer (Path B) ") + ((Scene::ShowTileCollisionFlag == 2) ? "(On)" : "(Off)"),
		std::string("Object Region Viewer ") + (Scene::ShowObjectRegions ? "(On)" : "(Off)") };
	for (size_t i = 0, y = 86; i < std::size(labels); i++, y += 14)
		DrawDevString(labels[i].c_str(), 160, y, ALIGN_LEFT, DevMenu.SubSelection == i);

	int actionUp = InputManager::GetActionID("Up");
	int actionDown = InputManager::GetActionID("Down");

	if ((actionUp != -1 && (InputManager::IsActionPressedByAny(actionUp) || (InputManager::IsActionHeldByAny(actionUp) && !DevMenu.Timer))) ||
		(actionDown != -1 && (InputManager::IsActionPressedByAny(actionDown) || (InputManager::IsActionHeldByAny(actionDown) && !DevMenu.Timer)))) {

		DevMenu.SubSelection = (DevMenu.SubSelection + (((actionUp != -1 && InputManager::IsActionPressedByAny(actionUp)) || (actionUp != -1 && InputManager::IsActionHeldByAny(actionUp))) ? -1 : 1) + (int)std::size(labels)) % (int)std::size(labels);
		DevMenu.Timer = 8;
	}

	if (DevMenu.Timer > 0) DevMenu.Timer = ++DevMenu.Timer & 7;

	bool confirm = false;
	for (const char* action : { "A", "Start" }) {
		int actionID = InputManager::GetActionID(action);
		if (actionID != -1 && InputManager::IsActionPressedByAny(actionID)) {
			confirm = true;
			break;
		}
	}

	if (confirm) {
		switch (DevMenu.SubSelection) {
		case 0: // Video Menu
			DevMenu.State = DevMenu_VideoMenu;
			DevMenu.SubSelection = 0;
			break;
		case 1: // Audio Menu
			DevMenu.State = DevMenu_AudioMenu;
			DevMenu.SubSelection = 0;
			break;
		case 2: // Performance Viewer
			ViewPerformance = !ViewPerformance;
			break;
		case 3: // Tile Collision Viewer (Path A)
			Scene::ShowTileCollisionFlag != 1 ? Scene::ShowTileCollisionFlag = 1 : Scene::ShowTileCollisionFlag = 0;
			Application::UpdateWindowTitle();
			break;
		case 4: // Tile Collision Viewer (Path B)
			Scene::ShowTileCollisionFlag != 2 ? Scene::ShowTileCollisionFlag = 2 : Scene::ShowTileCollisionFlag = 0;
			Application::UpdateWindowTitle();
			break;
		case 5: // Object Region Viewer
			Scene::ShowObjectRegions ^= 1;
			Application::UpdateWindowTitle();
			break;
		}
	}
	else {
		int actionB = InputManager::GetActionID("B");
		if (actionB != -1 && InputManager::IsActionPressedByAny(actionB)) {
			DevMenu.State = DevMenu_MainMenu;
			DevMenu.SubSelection = 0;
		}
	}
}

void Application::DevMenu_VideoMenu() {
	DevMenu_DrawTitleBar();

	DrawDevString("Change video settings...", Application::WindowWidth / 2, 50, ALIGN_CENTER, true);
	DrawRectangle((Application::WindowWidth / 2) - 140, 82, 280, 124, 0x000000, 0xC0);

	const char* labels[] = { "Window Scale", "Fullscreen", "Borderless" };
	for (size_t i = 0; i < std::size(labels); i++)
		DrawDevString(labels[i], (Application::WindowWidth / 2) - 124, 105 + (i * 16), ALIGN_LEFT, DevMenu.SubSelection == i);

	DrawDevString("Confirm", Application::WindowWidth / 2, 161, ALIGN_CENTER, DevMenu.SubSelection == std::size(labels));
	DrawDevString("Cancel", Application::WindowWidth / 2, 177, ALIGN_CENTER, DevMenu.SubSelection == std::size(labels) + 1);

	char scale[2];
	snprintf(scale, sizeof scale, "%d", DevMenu.WindowScale);
	DrawDevString(scale, (Application::WindowWidth / 2) + 80, 105, ALIGN_CENTER, DevMenu.SubSelection == 0);
	DrawDevString(DevMenu.Fullscreen ? "YES" : "NO", (Application::WindowWidth / 2) + 80, 121, ALIGN_CENTER, DevMenu.SubSelection == 1);
	DrawDevString(DevMenu.WindowBorderless ? "YES" : "NO", (Application::WindowWidth / 2) + 80, 137, ALIGN_CENTER, DevMenu.SubSelection == 2);

	int actionUp = InputManager::GetActionID("Up");
	int actionDown = InputManager::GetActionID("Down");

	if ((actionUp != -1 && (InputManager::IsActionPressedByAny(actionUp) || (InputManager::IsActionHeldByAny(actionUp) && !DevMenu.Timer))) ||
		(actionDown != -1 && (InputManager::IsActionPressedByAny(actionDown) || (InputManager::IsActionHeldByAny(actionDown) && !DevMenu.Timer)))) {

		DevMenu.SubSelection = (DevMenu.SubSelection + (((actionUp != -1 && InputManager::IsActionPressedByAny(actionUp)) || (actionUp != -1 && InputManager::IsActionHeldByAny(actionUp))) ? -1 : 1) + (int)std::size(labels) + 2) % ((int)std::size(labels) + 2);
		DevMenu.Timer = 8;
	}

	if (DevMenu.Timer > 0) DevMenu.Timer = ++DevMenu.Timer & 7;

	switch (DevMenu.SubSelection) {
	case 0: { // Scale
		const char* dir = InputManager::IsActionPressedByAny(InputManager::GetActionID("Left")) ? "Left" :
			InputManager::IsActionPressedByAny(InputManager::GetActionID("Right")) ? "Right" : nullptr;
		if (dir) {
			DevMenu.WindowScale += (dir[0] == 'L') ? -1 : 1;
			DevMenu.WindowScale = (DevMenu.WindowScale + 7) % 8 + 1;
		}
		break;
	}
	case 1: // Fullscreen
	case 2: { // Borderless 
		if (InputManager::IsActionPressedByAny(InputManager::GetActionID("Left")) ||
			InputManager::IsActionPressedByAny(InputManager::GetActionID("Right"))) {
			DevMenu.SubSelection == 1 ? DevMenu.Fullscreen ^= 1 : DevMenu.WindowBorderless ^= 1;
		}
		break;
	}
	case 3: // Confirm
	case 4: { // Cancel
		bool confirm = false;
		for (const char* action : { "A", "Start" }) {
			int actionID = InputManager::GetActionID(action);
			if (actionID != -1 && InputManager::IsActionPressedByAny(actionID)) {
				confirm = true;
				break;
			}
		}

		if (confirm) {
			if (DevMenu.SubSelection == 3) { // Confirm
				Application::SetWindowFullscreen(DevMenu.Fullscreen);
				Application::SetWindowBorderless(DevMenu.WindowBorderless);
				if (Application::IsWindowResizeable()) {
					Application::SetWindowScale(DevMenu.WindowScale);
				}
			}
			else { // Cancel
				DevMenu.WindowScale = Application::WindowScale;
				DevMenu.Fullscreen = Application::WindowFullscreen;
				DevMenu.WindowBorderless = Application::WindowBorderless;
			}
			DevMenu.State = DevMenu_SettingsMenu;
			DevMenu.SubSelection = 0;
		}
		break;
	}
	}
}

void Application::DevMenu_AudioMenu() {
	DevMenu_DrawTitleBar();

	DrawDevString("Change audio settings...", Application::WindowWidth / 2, 50, ALIGN_CENTER, true);
	DrawRectangle((Application::WindowWidth / 2) - 140, 82, 280, 124, 0x000000, 0xC0);

	const char* labels[] = { "Master Volume", "Music Volume", "Sound Volume" };
	const char* keys[] = { "masterVolume", "musicVolume", "soundVolume" };
	for (size_t i = 0; i < std::size(labels); i++)
		DrawDevString(labels[i], (Application::WindowWidth / 2) - 124, 105 + (i * 16), ALIGN_LEFT, DevMenu.SubSelection == i);

	DrawDevString("Confirm", Application::WindowWidth / 2, 169, ALIGN_CENTER, DevMenu.SubSelection == std::size(labels));

	for (size_t i = 0, y = 105; i < std::size(labels); i++, y += 16) {
		DrawRectangle((Application::WindowWidth / 2.0) + 22.0, y, 104.0, 15.0, 0x303030, 0xC0);
		DrawRectangle((Application::WindowWidth / 2.0) + 24.0, y + 2, (float)((int*)&Application::MasterVolume)[i], 11.0, 0xFFFFFF, 0xC0);
	}

	int actionUp = InputManager::GetActionID("Up");
	int actionDown = InputManager::GetActionID("Down");

	if ((actionUp != -1 && (InputManager::IsActionPressedByAny(actionUp) || (InputManager::IsActionHeldByAny(actionUp) && !DevMenu.Timer))) ||
		(actionDown != -1 && (InputManager::IsActionPressedByAny(actionDown) || (InputManager::IsActionHeldByAny(actionDown) && !DevMenu.Timer)))) {

		DevMenu.SubSelection = (DevMenu.SubSelection + ((InputManager::IsActionPressedByAny(actionUp) || InputManager::IsActionHeldByAny(actionUp)) ? -1 : 1) + (int)std::size(labels) + 1) % ((int)std::size(labels) + 1);
		DevMenu.Timer = 8;
	}

	if (DevMenu.Timer > 0) DevMenu.Timer = ++DevMenu.Timer & 7;

	const char* volumeDirs[] = { "Left", "Right" };
	for (const char* dir : volumeDirs) {
		int actionID = InputManager::GetActionID(dir);
		if (actionID != -1 && (InputManager::IsActionPressedByAny(actionID) || (InputManager::IsActionHeldByAny(actionID)))) {
			if (DevMenu.SubSelection < std::size(labels)) {
				int& volume = DevMenu.SubSelection == 0 ? Application::MasterVolume : DevMenu.SubSelection == 1 ? Application::MusicVolume : Application::SoundVolume;
				volume = std::clamp(volume + (dir[0] == 'L' ? -1 : 1), 0, 100);
				DevMenu.Timer = 8;
				Application::Settings->SetInteger("audio", keys[DevMenu.SubSelection], volume);
			}
		}
	}

	bool confirm = false;
	for (const char* action : { "A", "Start" }) {
		int actionID = InputManager::GetActionID(action);
		if (actionID != -1 && InputManager::IsActionPressedByAny(actionID)) {
			confirm = true;
			break;
		}
	}

	if (DevMenu.SubSelection == std::size(labels) && confirm) {
		Application::LoadAudioSettings();
		DevMenu.State = DevMenu_SettingsMenu;
		DevMenu.SubSelection = 1;
		DevMenu.Timer = 1;
	}
}