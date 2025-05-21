#ifndef ENGINE_APPLICATION_H
#define ENGINE_APPLICATION_H

#include <Engine/Audio/AudioManager.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/Version.h>
#include <Engine/InputManager.h>
#include <Engine/Math/Math.h>
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

	static void LogEngineVersion();
	static void LogSystemInfo();
	static void MakeEngineVersion();
	static bool ValidateIdentifier(const char* string);
	static char* GenerateIdentifier(const char* string);
	static bool
	ValidateAndSetIdentifier(const char* name, const char* id, char* dest, size_t destSize);
	static void CreateWindow();
	static void Restart(bool keepScene);
	static void LoadVideoSettings();
	static void LoadAudioSettings();
	static void LoadKeyBinds();
	static void LoadDevSettings();
	static bool ValidateAndSetIdentifier(const char* name, const char* id, char* dest);
	static void PollEvents();
	static void RunFrame(int runFrames);
	static void RunFrameCallback(void* p);
	static void DelayFrame();
	static void StartGame(const char* startingScene);
	static void LoadGameConfig();
	static void DisposeGameConfig();
	static string ParseGameVersion(XMLNode* versionNode);
	static void LoadGameInfo();
	static int HandleAppEvents(void* data, SDL_Event* event);
	static void DrawDevString(const char* string, int x, int y, int align, bool isSelected);
	static void OpenDevMenu();
	static void CloseDevMenu();
	static void SetBlendColor(int color);
	static void DrawRectangle(float x, float y, float width, float height, int color, int alpha, bool screenRelative);
	static void DevMenu_DrawMainMenu();
	static void DevMenu_DrawTitleBar();
	static void DevMenu_MainMenu();
	static void DevMenu_CategorySelectMenu();
	static void DevMenu_SceneSelectMenu();
	static void DevMenu_SettingsMenu();
	static void DevMenu_VideoMenu();
	static void DevMenu_AudioMenu();
	static void DevMenu_InputMenu();
	static void DevMenu_DebugMenu();
	static void DevMenu_ModsMenu();

public:
	static vector<char*> CmdLineArgs;
	static INI* Settings;
	static char SettingsFile[MAX_PATH_LENGTH];
	static XMLNode* GameConfig;
	static int TargetFPS;
	static float CurrentFPS;
	static bool Running;
	static bool FirstFrame;
	static SDL_Window* Window;
	static char WindowTitle[256];
	static int WindowWidth;
	static int WindowHeight;
	static int WindowScale;
	static bool WindowFullscreen;
	static bool WindowBorderless;
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

	static vector<ViewableVariable*> ViewableVariableList;
	static DeveloperMenu DevMenu;
	static int DeveloperDarkFont;
	static int DeveloperLightFont;

	static void Init(int argc, char* args[]);
	static void SetTargetFrameRate(int targetFPS);
	static bool IsPC();
	static bool IsMobile();
	static const char* GetDeveloperIdentifier();
	static const char* GetGameIdentifier();
	static const char* GetSavesDir();
	static const char* GetPreferencesDir();
	static void GetPerformanceSnapshot();
	static void SetWindowTitle(const char* title);
	static void UpdateWindowTitle();
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
	static void LoadSceneInfo(int activeCategory, int currentSceneNum, bool keepScene);
	static void InitPlayerControls();
	static bool LoadSettings(const char* filename);
	static void ReadSettings();
	static void ReloadSettings();
	static void ReloadSettings(const char* filename);
	static void InitSettings(const char* filename);
	static void SaveSettings();
	static void SaveSettings(const char* filename);
	static void SetSettingsFilename(const char* filename);
	static void AddViewableVariable(const char* name, void* value, int type, int min, int max);
	static Uint16* UTF8toUTF16(const char* utf8string);
	static int LoadDevFont(const char* fileName);
};

#endif /* ENGINE_APPLICATION_H */
