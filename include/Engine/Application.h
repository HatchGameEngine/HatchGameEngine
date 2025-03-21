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

class Application {
private:
	static void MakeEngineVersion();
	static void LogEngineVersion();
	static void LogSystemInfo();
	static void Restart();
	static void CreateWindow();
	static void LoadVideoSettings();
	static void LoadAudioSettings();
	static void LoadKeyBinds();
	static void LoadDevSettings();
	static void PollEvents();
	static void RunFrame(int runFrames);
	static void RunFrameCallback(void* p);
	static void DelayFrame();
	static void LoadGameConfig();
	static void DisposeGameConfig();
	static string ParseGameVersion(XMLNode* versionNode);
	static void LoadGameInfo();
	static int HandleAppEvents(void* data, SDL_Event* event);

public:
	static vector<char*> CmdLineArgs;
	static INI* Settings;
	static char SettingsFile[MAX_PATH_LENGTH];
	static XMLNode* GameConfig;
	static int TargetFPS;
	static float CurrentFPS;
	static bool Running;
	static bool FirstFrame;
	static bool GameStart;
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
	static int UpdatesPerFrame;
	static bool Stepper;
	static bool Step;
	static int MasterVolume;
	static int MusicVolume;
	static int SoundVolume;
	static bool DevMenuActivated;
	static bool DevConvertModels;
	static bool AllowCmdLineSceneLoad;

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
	static void LoadSceneInfo();
	static void InitPlayerControls();
	static bool LoadSettings(const char* filename);
	static void ReadSettings();
	static void ReloadSettings();
	static void ReloadSettings(const char* filename);
	static void InitSettings(const char* filename);
	static void SaveSettings();
	static void SaveSettings(const char* filename);
	static void SetSettingsFilename(const char* filename);
};

#endif /* ENGINE_APPLICATION_H */
