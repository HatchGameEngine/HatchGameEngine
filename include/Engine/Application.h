#ifndef ENGINE_APPLICATION_H
#define ENGINE_APPLICATION_H

#include <Engine/Audio/AudioManager.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/Version.h>
#include <Engine/InputManager.h>
#include <Engine/Math/Math.h>
#include <Engine/Platforms/Capability.h>
#include <Engine/ResourceTypes/Font.h>
#include <Engine/Scene.h>
#include <Engine/TextFormats/INI/INI.h>
#include <Engine/TextFormats/XML/XMLNode.h>
#include <Engine/TextFormats/XML/XMLParser.h>

#define DEFAULT_GAME_TITLE "Hatch Game Engine"
#define DEFAULT_GAME_SHORT_TITLE DEFAULT_GAME_TITLE
#define DEFAULT_GAME_VERSION "1.0"
#define DEFAULT_GAME_DESCRIPTION "Cluck cluck I'm a chicken"
#define DEFAULT_GAME_IDENTIFIER "hatch"

#define DEFAULT_SETTINGS_FILENAME "config://config.ini"

#define DEFAULT_SAVES_DIR "saves"

class Application {
private:
	static char GameIdentifier[256];
	static char DeveloperIdentifier[256];
	static char SavesDir[256];
	static char PreferencesDir[256];

	static std::unordered_map<std::string, Capability> CapabilityMap;

	static void LogEngineVersion();
	static void LogSystemInfo();
	static void MakeEngineVersion();
	static void RemoveCapability(std::string capability);
	static bool ValidateIdentifier(const char* string);
	static char* GenerateIdentifier(const char* string);
	static bool
	ValidateAndSetIdentifier(const char* name, const char* id, char* dest, size_t destSize);
	static void UnloadDefaultFont();
	static void CreateWindow();
	static void EndGame();
	static void UnloadGame();
	static void Restart();
	static void LoadVideoSettings();
	static void LoadAudioSettings();
	static void LoadKeyBinds();
	static void LoadDevSettings();
	static bool ValidateAndSetIdentifier(const char* name, const char* id, char* dest);
	static void PollEvents();
	static void RunFrame(int runFrames);
	static void RunFrameCallback(void* p);
	static void DrawPerformance();
	static void DelayFrame();
	static void StartGame(const char* startingScene);
	static void LoadGameConfig();
	static void DisposeGameConfig();
	static string ParseGameVersion(XMLNode* versionNode);
	static void InitGameInfo();
	static void LoadGameInfo();
	static void DisposeSettings();
	static int HandleAppEvents(void* data, SDL_Event* event);

public:
	static vector<std::string> CmdLineArgs;
	static std::vector<std::string> DefaultFontList;
	static INI* Settings;
	static char SettingsFile[MAX_PATH_LENGTH];
	static XMLNode* GameConfig;
	static Font* DefaultFont;
	static int TargetFPS;
	static float CurrentFPS;
	static bool Running;
	static bool FirstFrame;
	static SDL_Window* Window;
	static char WindowTitle[256];
	static int WindowWidth;
	static int WindowHeight;
	static int DefaultMonitor;
	static Platforms Platform;
	static char EngineVersion[256];
	static char GameTitle[256];
	static char GameTitleShort[256];
	static char GameVersion[256];
	static char GameDescription[256];
	static char GameDeveloper[256];
	static int UpdatesPerFrame;
	static int FrameSkip;
	static bool Stepper;
	static bool Step;
	static int MasterVolume;
	static int MusicVolume;
	static int SoundVolume;
	static bool DevMenuActivated;
	static bool DevConvertModels;
	static bool AllowCmdLineSceneLoad;

	static void Init(int argc, char* args[]);
	static void InitScripting();
	static void SetTargetFrameRate(int targetFPS);
	static bool IsPC();
	static bool IsMobile();
	static void AddCapability(std::string capability, int value);
	static void AddCapability(std::string capability, float value);
	static void AddCapability(std::string capability, bool value);
	static void AddCapability(std::string capability, std::string value);
	static Capability GetCapability(std::string capability);
	static bool HasCapability(std::string capability);
	static const char* GetDeveloperIdentifier();
	static const char* GetGameIdentifier();
	static const char* GetSavesDir();
	static const char* GetPreferencesDir();
	static void LoadDefaultFont();
	static void GetPerformanceSnapshot();
	static void SetWindowTitle(const char* title);
	static void UpdateWindowTitle();
	static bool SetNextGame(const char* path,
		const char* startingScene,
		std::vector<std::string>* cmdLineArgs);
	static bool ChangeGame(const char* path);
	static void SetMasterVolume(int volume);
	static void SetMusicVolume(int volume);
	static void SetSoundVolume(int volume);
	static bool IsWindowResizeable();
	static void SetWindowSize(int window_w, int window_h);
	static bool GetWindowFullscreen();
	static void SetWindowFullscreen(bool isFullscreen);
	static void SetWindowBorderless(bool isBorderless);
	static int GetKeyBind(int bind);
	static void SetKeyBind(int bind, int key);
	static void Run(int argc, char* args[]);
	static void Cleanup();
	static void TerminateScripting();
	static void LoadSceneInfo();
	static void InitPlayerControls();
	static bool LoadSettings(const char* filename);
	static void ReadSettings();
	static void ReloadSettings();
	static void ReloadSettings(const char* filename);
	static void InitSettings();
	static void SaveSettings();
	static void SaveSettings(const char* filename);
	static void SetSettingsFilename(const char* filename);
};

#endif /* ENGINE_APPLICATION_H */
