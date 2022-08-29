#ifndef ENGINE_APPLICATION_H
#define ENGINE_APPLICATION_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/InputManager.h>
#include <Engine/Audio/AudioManager.h>
#include <Engine/Scene.h>
#include <Engine/Math/Math.h>
#include <Engine/TextFormats/INI.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/TextFormats/XML/XMLNode.h>

class Application {
private:
    static void Restart();
    static void LoadAudioSettings();
    static void LoadDevSettings();
    static void PollEvents();
    static void RunFrame(void* p);
    static void DelayFrame();
    static void Cleanup();
    static void LoadGameConfig();
    static void DisposeGameConfig();
    static int HandleAppEvents(void* data, SDL_Event* event);

public:
    static INI*        Settings;
    static char        SettingsFile[4096];
    static XMLNode*    GameConfig;
    static float       FPS;
    static bool        Running;
    static bool        GameStart;
    static SDL_Window* Window;
    static char        WindowTitle[256];
    static int         WindowWidth;
    static int         WindowHeight;
    static Platforms   Platform;
    static int         UpdatesPerFrame;
    static bool        Stepper;
    static bool        Step;
    static int         MasterVolume;
    static int         MusicVolume;
    static int         SoundVolume;

    static void Init(int argc, char* args[]);
    static void GetPerformanceSnapshot();
    static void UpdateWindowTitle();
    static void SetMasterVolume(int volume);
    static void SetMusicVolume(int volume);
    static void SetSoundVolume(int volume);
    static void Run(int argc, char* args[]);
    static void LoadSettings();
    static void SaveSettings();
};

#endif /* ENGINE_APPLICATION_H */
