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

namespace Application {
//private:
	void MakeEngineVersion();
	void LogEngineVersion();
	void LogSystemInfo();
	void Restart();
	void CreateWindow();
	void LoadVideoSettings();
	void LoadAudioSettings();
	void LoadKeyBinds();
	void LoadDevSettings();
	void PollEvents();
	void RunFrame(int runFrames);
	void RunFrameCallback(void* p);
	void DelayFrame();
	void LoadGameConfig();
	void DisposeGameConfig();
	string ParseGameVersion(XMLNode* versionNode);
	void LoadGameInfo();
	int HandleAppEvents(void* data, SDL_Event* event);

//public:
	extern vector<char*> CmdLineArgs;
	extern INI* Settings;
	extern char SettingsFile[MAX_PATH_LENGTH];
	extern XMLNode* GameConfig;
	extern int TargetFPS;
	extern float CurrentFPS;
	extern bool Running;
	extern bool FirstFrame;
	extern bool GameStart;
	extern bool PortableMode;
	extern SDL_Window* Window;
	extern char WindowTitle[256];
	extern int WindowWidth;
	extern int WindowHeight;
	extern int DefaultMonitor;
	extern Platforms Platform;
	extern char EngineVersion[256];
	extern char GameTitle[256];
	extern char GameTitleShort[256];
	extern char GameVersion[256];
	extern char GameDescription[256];
	extern int UpdatesPerFrame;
	extern int FrameSkip;
	extern bool Stepper;
	extern bool Step;
	extern int MasterVolume;
	extern int MusicVolume;
	extern int SoundVolume;
	extern bool DevMenuActivated;
	extern bool DevConvertModels;
	extern bool AllowCmdLineSceneLoad;

	void Init(int argc, char* args[]);
	void SetTargetFrameRate(int targetFPS);
	bool IsPC();
	bool IsMobile();
	const char* GetDeveloperIdentifier();
	const char* GetGameIdentifier();
	const char* GetSavesDir();
	const char* GetPreferencesDir();
	void GetPerformanceSnapshot();
	void SetWindowTitle(const char* title);
	void UpdateWindowTitle();
	void SetMasterVolume(int volume);
	void SetMusicVolume(int volume);
	void SetSoundVolume(int volume);
	bool IsWindowResizeable();
	void SetWindowSize(int window_w, int window_h);
	bool GetWindowFullscreen();
	void SetWindowFullscreen(bool isFullscreen);
	void SetWindowBorderless(bool isBorderless);
	int GetKeyBind(int bind);
	void SetKeyBind(int bind, int key);
	void Run(int argc, char* args[]);
	void Cleanup();
	void LoadSceneInfo();
	void InitPlayerControls();
	bool LoadSettings(const char* filename);
	void ReadSettings();
	void ReloadSettings();
	void ReloadSettings(const char* filename);
	void InitSettings(const char* filename);
	void SaveSettings();
	void SaveSettings(const char* filename);
	void SetSettingsFilename(const char* filename);
};

#endif /* ENGINE_APPLICATION_H */
