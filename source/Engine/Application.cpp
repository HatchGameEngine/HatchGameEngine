#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/InputManager.h>
#include <Engine/Audio/AudioManager.h>
#include <Engine/Scene.h>
#include <Engine/Math/Math.h>
#include <Engine/TextFormats/INI.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/TextFormats/XML/XMLNode.h>

class Application {
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
};
#endif

#include <Engine/Application.h>
#include <Engine/Graphics.h>

#include <Engine/Bytecode/BytecodeObjectManager.h>
#include <Engine/Bytecode/GarbageCollector.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Diagnostics/MemoryPools.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/TextFormats/XML/XMLNode.h>
#include <Engine/Utilities/StringUtils.h>

#include <Engine/Media/MediaSource.h>
#include <Engine/Media/MediaPlayer.h>

#ifdef IOS
extern "C" {
    #include <Engine/Platforms/iOS/MediaPlayer.h>
}
#endif

#if   WIN32
    Platforms Application::Platform = Platforms::Windows;
#elif MACOSX
    Platforms Application::Platform = Platforms::MacOSX;
#elif LINUX
    Platforms Application::Platform = Platforms::Linux;
#elif UBUNTU
    Platforms Application::Platform = Platforms::Ubuntu;
#elif SWITCH
    Platforms Application::Platform = Platforms::Switch;
#elif PLAYSTATION
    Platforms Application::Platform = Platforms::Playstation;
#elif XBOX
    Platforms Application::Platform = Platforms::Xbox;
#elif ANDROID
    Platforms Application::Platform = Platforms::Android;
#elif IOS
    Platforms Application::Platform = Platforms::iOS;
#else
    Platforms Application::Platform = Platforms::Default;
#endif

INI*        Application::Settings = NULL;
char        Application::SettingsFile[4096];

XMLNode*    Application::GameConfig = NULL;

float       Application::FPS = 60.f;
int         TargetFPS = 60;
bool        Application::Running = false;
bool        Application::GameStart = false;

SDL_Window* Application::Window = NULL;
char        Application::WindowTitle[256];
int         Application::WindowWidth = 640;
int         Application::WindowHeight = 480;

int         Application::UpdatesPerFrame = 1;
bool        Application::Stepper = false;
bool        Application::Step = false;

int         Application::MasterVolume = 100;
int         Application::MusicVolume = 100;
int         Application::SoundVolume = 100;

char        StartingScene[256];

ISprite*    DEBUG_fontSprite = NULL;
void        DEBUG_DrawText(char* text, float x, float y) {
    for (char* i = text; *i; i++) {
        Graphics::DrawSprite(DEBUG_fontSprite, 0, (int)*i, x, y, false, false, 1.0f, 1.0f, 0.0f);
        x += 14; // DEBUG_fontSprite->Animations[0].Frames[(int)*i].ID;
    }
}

PUBLIC STATIC void Application::Init(int argc, char* args[]) {
    Log::Init();
    MemoryPools::Init();

    SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");

    #ifdef IOS
    // SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown"); // iOS only
    // SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback"); // Background Playback
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "1");
    iOS_InitMediaPlayer();
    #endif

    #ifdef ANDROID
    SDL_SetHint(SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH, "1");
    #endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
        Log::Print(Log::LOG_INFO, "SDL_Init failed with error: %s", SDL_GetError());
    }

    SDL_SetEventFilter(Application::HandleAppEvents, NULL);

    strcpy(Application::SettingsFile, "config.ini");
    Application::LoadSettings();

    Graphics::ChooseBackend();

    Application::Settings->GetBool("dev", "writeToFile", &Log::WriteToFile);

    bool allowRetina = false;
    Application::Settings->GetBool("display", "retina", &allowRetina);

    int defaultMonitor = 0;
    Application::Settings->GetInteger("display", "defaultMonitor", &defaultMonitor);

    Uint32 window_flags = 0;
    window_flags |= SDL_WINDOW_SHOWN;
    window_flags |= Graphics::GetWindowFlags();
    if (allowRetina)
        window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    Application::Window = SDL_CreateWindow(NULL,
        SDL_WINDOWPOS_CENTERED_DISPLAY(defaultMonitor), SDL_WINDOWPOS_CENTERED_DISPLAY(defaultMonitor),
        Application::WindowWidth, Application::WindowHeight, window_flags);

    if (Application::Platform == Platforms::iOS) {
        // SDL_SetWindowFullscreen(Application::Window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_SetWindowFullscreen(Application::Window, SDL_WINDOW_FULLSCREEN);
    }
    else if (Application::Platform == Platforms::Switch) {
        SDL_SetWindowFullscreen(Application::Window, SDL_WINDOW_FULLSCREEN);
        // SDL_GetWindowSize(Application::Window, &Application::WIDTH, &Application::HEIGHT);
        AudioManager::MasterVolume = 0.25;

        #ifdef SWITCH
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(0, 1 - appletGetOperationMode(), &mode);
        // SDL_SetWindowDisplayMode(Application::Window, &mode);
        Log::Print(Log::LOG_INFO, "Display Mode: %i x %i", mode.w, mode.h);
        #endif
    }

    // Initialize subsystems
    Math::Init();
    Graphics::Init();
    if (argc > 1 && !!StringUtils::StrCaseStr(args[1], ".hatch"))
        ResourceManager::Init(args[1]);
    else
        ResourceManager::Init(NULL);
    AudioManager::Init();
    InputManager::Init();
    Clock::Init();

    Application::LoadGameConfig();
    Application::LoadAudioSettings();
    Application::LoadDevSettings();

    Application::DisposeGameConfig();

    switch (Application::Platform) {
        case Platforms::Windows:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Windows"); break;
        case Platforms::MacOSX:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "MacOS"); break;
        case Platforms::Linux:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Linux"); break;
        case Platforms::Ubuntu:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Ubuntu"); break;
        case Platforms::Switch:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Nintendo Switch"); break;
        case Platforms::Playstation:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Playstation"); break;
        case Platforms::Xbox:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Xbox"); break;
        case Platforms::Android:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Android"); break;
        case Platforms::iOS:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "iOS"); break;
        default:
            Log::Print(Log::LOG_INFO, "Current Platform: %s", "Unknown"); break;
    }

    memset(Application::WindowTitle, 0, sizeof(Application::WindowTitle));
    sprintf(Application::WindowTitle, "%s", "Hatch Game Engine");
    Application::UpdateWindowTitle();

    Running = true;
}

bool    AutomaticPerformanceSnapshots = false;
double  AutomaticPerformanceSnapshotFrameTimeThreshold = 20.0;
double  AutomaticPerformanceSnapshotLastTime = 0.0;
double  AutomaticPerformanceSnapshotMinInterval = 5000.0; // 5 seconds

int     MetricFrameCounterTime = 0;
double  MetricEventTime = -1;
double  MetricAfterSceneTime = -1;
double  MetricPollTime = -1;
double  MetricUpdateTime = -1;
double  MetricClearTime = -1;
double  MetricRenderTime = -1;
double  MetricFPSCounterTime = -1;
double  MetricPresentTime = -1;
double  MetricFrameTime = 0.0;
vector<ObjectList*> ListList;
PUBLIC STATIC void Application::GetPerformanceSnapshot() {
    if (Scene::ObjectLists) {
        // General Performance Snapshot
        double types[] = {
            MetricEventTime,
            MetricAfterSceneTime,
            MetricPollTime,
            MetricUpdateTime,
            MetricClearTime,
            MetricRenderTime,
            // MetricFPSCounterTime,
            MetricPresentTime,
            0.0,
            MetricFrameTime,
            FPS,
        };
        const char* typeNames[] = {
            "Event Polling:         %8.3f ms",
            "Garbage Collector:     %8.3f ms",
            "Input Polling:         %8.3f ms",
            "Entity Update:         %8.3f ms",
            "Clear Time:            %8.3f ms",
            "World Render Commands: %8.3f ms",
            // "FPS Counter Time:      %8.3f ms",
            "Frame Present Time:    %8.3f ms",
            "==================================",
            "Frame Total Time:      %8.3f ms",
            "FPS:                   %11.3f",
        };

        ListList.clear();
        Scene::ObjectLists->WithAll([](Uint32, ObjectList* list) -> void {
            if ((list->AverageUpdateTime > 0.0 && list->AverageUpdateItemCount > 0) ||
                (list->AverageRenderTime > 0.0 && list->AverageRenderItemCount > 0))
                ListList.push_back(list);
        });
        std::sort(ListList.begin(), ListList.end(), [](ObjectList* a, ObjectList* b) -> bool {
            return
                a->AverageUpdateTime * a->AverageUpdateItemCount + a->AverageRenderTime * a->AverageRenderItemCount >
                b->AverageUpdateTime * b->AverageUpdateItemCount + b->AverageRenderTime * b->AverageRenderItemCount;
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
                    sprintf(layerText, "%s""     > %24s:   %8.3f ms\n", layerText,
                        layer->Name, Scene::PERF_ViewRender[i].LayerTileRenderTime[li]);
                    tilesTotal += Scene::PERF_ViewRender[i].LayerTileRenderTime[li];
                }
                Log::Print(Log::LOG_INFO, "View %d:\n"
                    "           - Render Setup:        %8.3f ms %s\n"
                    "           - Projection Setup:    %8.3f ms\n"
                    "           - Object RenderEarly:  %8.3f ms\n"
                    "           - Object Render:       %8.3f ms\n"
                    "           - Object RenderLate:   %8.3f ms\n"
                    "           - Layer Tiles Total:   %8.3f ms\n%s"
                    "           - Finish:              %8.3f ms\n"
                    "           - Total:               %8.3f ms",
                    i,
                    Scene::PERF_ViewRender[i].RenderSetupTime, Scene::PERF_ViewRender[i].RecreatedDrawTarget ? "(recreated draw target)" : "",
                    Scene::PERF_ViewRender[i].ProjectionSetupTime,
                    Scene::PERF_ViewRender[i].ObjectRenderEarlyTime,
                    Scene::PERF_ViewRender[i].ObjectRenderTime,
                    Scene::PERF_ViewRender[i].ObjectRenderLateTime,
                    tilesTotal, layerText,
                    Scene::PERF_ViewRender[i].RenderFinishTime,
                    Scene::PERF_ViewRender[i].RenderTime);
            }
            // double RenderSetupTime;
            // bool   RecreatedDrawTarget;
            // double ProjectionSetupTime;
            // double ObjectRenderEarlyTime;
            // double ObjectRenderTime;
            // double ObjectRenderLateTime;
            // double LayerTileRenderTime[32]; // MAX_LAYERS
            // double RenderFinishTime;
            // double RenderTime;
        }

        // Object Performance Snapshot
        double totalUpdate = 0.0;
        double totalRender = 0.0;
        Log::Print(Log::LOG_IMPORTANT, "Object Performance Snapshot:");
        for (size_t i = 0; i < ListList.size(); i++) {
            ObjectList* list = ListList[i];
            Log::Print(Log::LOG_INFO, "Object \"%s\":\n"
                "           - Avg Update %6.1f mcs (Total %6.1f mcs, Count %d)\n"
                "           - Avg Render %6.1f mcs (Total %6.1f mcs, Count %d)", list->ObjectName,
                list->AverageUpdateTime * 1000.0, list->AverageUpdateTime * list->AverageUpdateItemCount * 1000.0, (int)list->AverageUpdateItemCount,
                list->AverageRenderTime * 1000.0, list->AverageRenderTime * list->AverageRenderItemCount * 1000.0, (int)list->AverageRenderItemCount);

            totalUpdate += list->AverageUpdateTime * list->AverageUpdateItemCount * 1000.0;
            totalRender += list->AverageRenderTime * list->AverageRenderItemCount * 1000.0;
        }
        Log::Print(Log::LOG_WARN, "Total Update: %8.3f mcs / %1.3f ms", totalUpdate, totalUpdate / 1000.0);
        Log::Print(Log::LOG_WARN, "Total Render: %8.3f mcs / %1.3f ms", totalRender, totalRender / 1000.0);

        Log::Print(Log::LOG_IMPORTANT, "Garbage Size:");
        Log::Print(Log::LOG_INFO, "%u", (Uint32)GarbageCollector::GarbageSize);
    }
}

bool    DevMenu = false;
bool    ShowFPS = false;
bool    TakeSnapshot = false;
bool    DoNothing = false;
int     UpdatesPerFastForward = 4;

int     BenchmarkFrameCount = 0;
double  BenchmarkTickStart = 0.0;

double  Overdelay = 0.0;
double  FrameTimeStart = 0.0;
double  FrameTimeDesired = 1000.0 / TargetFPS;
PUBLIC STATIC void Application::UpdateWindowTitle() {
    char full[512];
    sprintf(full, "%s", Application::WindowTitle);

    bool paren = false;

    if (DevMenu && ResourceManager::UsingDataFolder) {
        if (!paren) {
            paren = true;
            strcat(full, " (");
        }
        else strcat(full, ", ");
        strcat(full, "using Resources folder");
    }
    if (DevMenu && ResourceManager::UsingModPack) {
        if (!paren) {
            paren = true;
            strcat(full, " (");
        }
        else strcat(full, ", ");
        strcat(full, "using Modpack");
    }

    if (UpdatesPerFrame != 1) {
        if (!paren) {
            paren = true;
            strcat(full, " (");
        }
        else strcat(full, ", ");
        strcat(full, "Frame Limit OFF");
    }

    switch (Scene::ShowTileCollisionFlag) {
        case 1:
            if (!paren) {
                paren = true;
                strcat(full, " (");
            }
            else strcat(full, ", ");
            strcat(full, "Viewing Path A");
            break;
        case 2:
            if (!paren) {
                paren = true;
                strcat(full, " (");
            }
            else strcat(full, ", ");
            strcat(full, "Viewing Path B");
            break;
    }

    if (Stepper) {
        if (!paren) {
            paren = true;
            strcat(full, " (");
        }
        else strcat(full, ", ");
        strcat(full, "Frame Stepper ON");
    }

    if (paren)
        strcat(full, ")");

    SDL_SetWindowTitle(Application::Window, full);
}

PRIVATE STATIC void Application::Restart() {
    // Reset FPS timer
    BenchmarkFrameCount = 0;

    Scene::Dispose();
    Graphics::SpriteSheetTextureMap->WithAll([](Uint32, Texture* tex) -> void {
        Graphics::DisposeTexture(tex);
    });
    Graphics::SpriteSheetTextureMap->Clear();

    Application::LoadSettings();
    Application::LoadGameConfig();

    Application::LoadAudioSettings();
    Application::LoadDevSettings();

    Application::DisposeGameConfig();
}

#define CLAMP_VOLUME(vol) \
    if (vol < 0) \
        vol = 0; \
    else if (vol > 100) \
        vol = 100

PUBLIC STATIC void Application::SetMasterVolume(int volume) {
    CLAMP_VOLUME(volume);

    Application::MasterVolume = volume;
    AudioManager::MasterVolume = Application::MasterVolume / 100.0f;
}
PUBLIC STATIC void Application::SetMusicVolume(int volume) {
    CLAMP_VOLUME(volume);

    Application::MusicVolume = volume;
    AudioManager::MusicVolume = Application::MusicVolume / 100.0f;
}
PUBLIC STATIC void Application::SetSoundVolume(int volume) {
    CLAMP_VOLUME(volume);

    Application::SoundVolume = volume;
    AudioManager::SoundVolume = Application::SoundVolume / 100.0f;
}

PRIVATE STATIC void Application::LoadAudioSettings() {
    int masterVolume = 100;
    int musicVolume = 100, soundVolume = 100;

    if (Application::GameConfig) {
        XMLNode* gameconfig = Application::GameConfig->children[0];
        char *xmlTokStr = NULL;

#define GET_VOLUME(node, var) \
        if (node->attributes.Exists("volume")) { \
            free(xmlTokStr); \
            xmlTokStr = XMLParser::TokenToString(node->attributes.Get("volume")); \
            if (StringUtils::ToNumber(&var, xmlTokStr)) { \
                CLAMP_VOLUME(var); \
            } \
        }

        for (size_t i = 0; i < gameconfig->children.size(); i++) {
            XMLNode* configItem = gameconfig->children[i];
            if (XMLParser::MatchToken(configItem->name, "audio")) {
                // Get master audio volume
                GET_VOLUME(configItem, masterVolume);

                // Get music and sound volume
                for (size_t j = 0; j < configItem->children.size(); j++) {
                    XMLNode* child = configItem->children[j];

                    if (XMLParser::MatchToken(child->name, "music")) {
                        GET_VOLUME(child, musicVolume);
                    } else if (XMLParser::MatchToken(child->name, "sound")) {
                        GET_VOLUME(child, soundVolume);
                    }
                }
            }
        }
    }

#undef GET_VOLUME

    INI* settings = Application::Settings;

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

PRIVATE STATIC void Application::LoadDevSettings() {
    Application::Settings->GetBool("dev", "devMenu", &DevMenu);
    Application::Settings->GetBool("dev", "viewPerformance", &ShowFPS);
    Application::Settings->GetBool("dev", "donothing", &DoNothing);
    Application::Settings->GetInteger("dev", "fastforward", &UpdatesPerFastForward);
}

PRIVATE STATIC void Application::PollEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT: {
                Running = false;
                break;
            }
            case SDL_KEYDOWN: {
                switch (e.key.keysym.sym) {
                    // Quit game (dev)
                    case SDLK_ESCAPE: {
                        if (DevMenu) {
                            Running = false;
                        }
                        break;
                    }
                    // Fullscreen
                    case SDLK_F4: {
                        if (SDL_GetWindowFlags(Application::Window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                            SDL_SetWindowFullscreen(Application::Window, 0);
                        }
                        else {
                            // SDL_SetWindowFullscreen(Application::Window, SDL_WINDOW_FULLSCREEN);
                            SDL_SetWindowFullscreen(Application::Window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                        }
                        int ww, wh;
                        SDL_GetWindowSize(Application::Window, &ww, &wh);
                        Graphics::Resize(ww, wh);
                        break;
                    }
                    // Restart application (dev)
                    case SDLK_F1: {
                        if (DevMenu) {
                            Application::Restart();

                            Scene::Init();
                            if (*StartingScene)
                                Scene::LoadScene(StartingScene);
                            Scene::Restart();
                            Application::UpdateWindowTitle();
                        }
                        break;
                    }
                    // Print performance snapshot (dev)
                    case SDLK_F2: {
                        if (DevMenu) {
                            for (size_t li = 0; li < Scene::Layers.size(); li++) {
                                SceneLayer layer = Scene::Layers[li];
                                Log::Print(Log::LOG_IMPORTANT, "%2d: %20s (Visible: %d, Width: %d, Height: %d, OffsetX: %d, OffsetY: %d, RelativeY: %d, ConstantY: %d, DrawGroup: %d, ScrollDirection: %d, Flags: %d)", li,
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
                        }
                        break;
                    }
                    // Print performance snapshot (dev)
                    case SDLK_F3: {
                        if (DevMenu) {
                            TakeSnapshot = true;
                        }
                        break;
                    }
                    // Restart scene (dev)
                    case SDLK_F5: {
                        if (DevMenu) {
                            Application::Restart();

                            char temp[256];
                            memcpy(temp, Scene::CurrentScene, 256);

                            Scene::Init();

                            memcpy(Scene::CurrentScene, temp, 256);
                            Scene::LoadScene(Scene::CurrentScene);

                            Scene::Restart();
                            Application::UpdateWindowTitle();
                        }
                        break;
                    }
                    // Restart scene without recompiling (dev)
                    case SDLK_F6: {
                        if (DevMenu) {
                            // Reset FPS timer
                            BenchmarkFrameCount = 0;

                            Scene::Restart();
                            Application::UpdateWindowTitle();
                        }
                        break;
                    }
                    // Enable update speedup (dev)
                    case SDLK_BACKSPACE: {
                        if (DevMenu) {
                            if (UpdatesPerFrame == 1)
                                UpdatesPerFrame = UpdatesPerFastForward;
                            else
                                UpdatesPerFrame = 1;

                            Application::UpdateWindowTitle();
                        }
                        break;
                    }
                    // Cycle view tile collision (dev)
                    case SDLK_F7: {
                        if (DevMenu) {
                            Scene::ShowTileCollisionFlag = (Scene::ShowTileCollisionFlag + 1) % 3;
                            Application::UpdateWindowTitle();
                        }
                        break;
                    }
                    // Cycle view object regions (dev)
                    case SDLK_F8: {
                        if (DevMenu) {
                            Scene::ShowObjectRegions ^= 1;
                            Application::UpdateWindowTitle();
                        }
                        break;
                    }
                    // Toggle frame stepper (dev)
                    case SDLK_F9: {
                        if (DevMenu) {
                            Stepper = !Stepper;
                            MetricFrameCounterTime = 0;
                            Application::UpdateWindowTitle();
                        }
                        break;
                    }
                    // Step frame (dev)
                    case SDLK_F10: {
                        if (DevMenu) {
                            Stepper = true;
                            Step = true;
                            MetricFrameCounterTime++;
                            Application::UpdateWindowTitle();
                        }
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
            case SDL_JOYDEVICEADDED: {
                int i = e.jdevice.which;
                InputManager::Controllers[i] = SDL_JoystickOpen(i);
                if (InputManager::Controllers[i])
                    Log::Print(Log::LOG_VERBOSE, "InputManager::Controllers[%d] opened.", i);

                if (InputManager::Controllers[i])
                    InputManager::ControllerHaptics[i] = SDL_HapticOpenFromJoystick((SDL_Joystick*)InputManager::Controllers[i]);

                if (InputManager::ControllerHaptics[i] && SDL_HapticRumbleInit((SDL_Haptic*)InputManager::ControllerHaptics[i]))
                    InputManager::ControllerHaptics[i] = NULL;
                break;
            }
            case SDL_JOYDEVICEREMOVED: {
                int i = e.jdevice.which;

                SDL_Joystick* joy = (SDL_Joystick*)InputManager::Controllers[i];

                SDL_JoystickClose(joy);

                InputManager::Controllers[i] = NULL;

                // if (InputManager::Controllers[i])
                //     InputManager::ControllerHaptics[i] = SDL_HapticOpenFromJoystick((SDL_Joystick*)InputManager::Controllers[i]);
                //
                // if (InputManager::ControllerHaptics[i] && SDL_HapticRumbleInit((SDL_Haptic*)InputManager::ControllerHaptics[i]))
                //     InputManager::ControllerHaptics[i] = NULL;
                break;
            }
        }
    }
}
PRIVATE STATIC void Application::RunFrame(void* p) {
    FrameTimeStart = Clock::GetTicks();

    // Event loop
    MetricEventTime = Clock::GetTicks();
    Application::PollEvents();
    MetricEventTime = Clock::GetTicks() - MetricEventTime;

    // BUG: Having Stepper on prevents the first
    //   frame of a new scene from Updating, but still rendering.
    if (*Scene::NextScene)
        Step = true;

    MetricAfterSceneTime = Clock::GetTicks();
    Scene::AfterScene();
    MetricAfterSceneTime = Clock::GetTicks() - MetricAfterSceneTime;

    if (DoNothing) goto DO_NOTHING;

    // Update
    for (int m = 0; m < UpdatesPerFrame; m++) {
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
            DEBUG_fontSprite->SpritesheetCount = 1;
            DEBUG_fontSprite->Spritesheets[0] = DEBUG_fontSprite->AddSpriteSheet("Debug/Font.png");
            cols = DEBUG_fontSprite->Spritesheets[0]->Width / 32;
            rows = DEBUG_fontSprite->Spritesheets[0]->Height / 32;

            DEBUG_fontSprite->ReserveAnimationCount(1);
            DEBUG_fontSprite->AddAnimation("Font?", 0, 0, cols * rows);
            for (int i = 0; i < cols * rows; i++) {
                DEBUG_fontSprite->AddFrame(0,
                    (i % cols) * 32,
                    (i / cols) * 32,
                    32, 32, 0, 0,
                    14);
            }

            Graphics::SetTextureInterpolation(original);
        }

        int ww, wh;
        char textBuffer[256];
        SDL_GetWindowSize(Application::Window, &ww, &wh);
        Graphics::SetViewport(0.0, 0.0, ww, wh);
        Graphics::UpdateOrthoFlipped(ww, wh);

        Graphics::SetBlendMode(BlendFactor_SRC_ALPHA, BlendFactor_INV_SRC_ALPHA, BlendFactor_SRC_ALPHA, BlendFactor_INV_SRC_ALPHA);

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
            struct { float r; float g; float b; } colors[8] = {
                { 1.0, 0.0, 0.0 },
                { 0.0, 1.0, 0.0 },
                { 0.0, 0.0, 1.0 },
                { 1.0, 1.0, 0.0 },
                { 0.0, 1.0, 1.0 },
                { 1.0, 0.0, 1.0 },
                { 1.0, 1.0, 1.0 },
                { 0.0, 0.0, 0.0 },
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
                snprintf(textBuffer, 256, "FPS: %03.1f", FPS);
                DEBUG_DrawText(textBuffer, 0.0, 0.0);
            Graphics::Restore();

            if (Application::Platform == Platforms::Android || true) {
                // Draw bar
                float total = 0.0001;
                for (int i = 0; i < typeCount; i++) {
                    if (types[i] < 0.0)
                        types[i] = 0.0;
                    total += types[i];
                }

                Graphics::Save();
                Graphics::Translate(infoPadding, 50.0, 0.0);
                    Graphics::SetBlendColor(0.0, 0.0, 0.0, 0.25);
                    Graphics::FillRectangle(0.0, 0.0f, infoW - infoPadding * 2, 30.0);
                Graphics::Restore();

                float rectx = 0.0;
                for (int i = 0; i < typeCount; i++) {
                    Graphics::Save();
                    Graphics::Translate(infoPadding, 50.0, 0.0);
                        if (i < 8)
                            Graphics::SetBlendColor(colors[i].r, colors[i].g, colors[i].b, 0.5);
                        else
                            Graphics::SetBlendColor(0.5, 0.5, 0.5, 0.5);
                        Graphics::FillRectangle(rectx, 0.0f, types[i] / total * (infoW - infoPadding * 2), 30.0);
                    Graphics::Restore();

                    rectx += types[i] / total * (infoW - infoPadding * 2);
                }

                // Draw list
                float listY = 90.0;
                float totalFrameCount = 0.0f;
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
                    Scene::ObjectLists->WithAll([infoPadding, listYPtr](Uint32, ObjectList* list) -> void {
                        char textBufferXXX[1024];
                        if (list->AverageUpdateItemCount > 0.0) {
                            Graphics::Save();
                            Graphics::Translate(infoPadding / 2.0, *listYPtr, 0.0);
                            Graphics::Scale(0.6, 0.6, 1.0);
                                // snprintf(textBufferXXX, 1024, "Object \"%s\": Avg Update %.1f mcs (Total %.1f mcs, Count %d)", list->ObjectName, list->AverageUpdateTime * 1000.0, list->AverageUpdateTime * list->AverageUpdateItemCount * 1000.0, (int)list->AverageUpdateItemCount);
                                snprintf(textBufferXXX, 1024, "Object \"%s\": Avg Render %.1f mcs (Total %.1f mcs, Count %d)", list->ObjectName, list->AverageRenderTime * 1000.0, list->AverageRenderTime * list->AverageRenderItemCount * 1000.0, (int)list->AverageRenderItemCount);
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
PRIVATE STATIC void Application::DelayFrame() {
    // HACK: MacOS V-Sync timing gets disabled if window is not visible
    if (!Graphics::VsyncEnabled || Application::Platform == Platforms::MacOSX) {
        double frameTime = Clock::GetTicks() - FrameTimeStart;
        double frameDurationRemainder = FrameTimeDesired - frameTime;
        if (frameDurationRemainder >= 0.0) {
            // NOTE: Delay duration will always be more than requested wait time.
            if (frameDurationRemainder > 1.0) {
                double delayStartTime = Clock::GetTicks();

                Clock::Delay(frameDurationRemainder - 1.0);

                double delayTime = Clock::GetTicks() - delayStartTime;
                Overdelay = delayTime - (frameDurationRemainder - 1.0);
            }

            // frameDurationRemainder = floor(frameDurationRemainder);
            // if (delayTime > frameDurationRemainder)
            //     printf("delayTime: %.3f   frameDurationRemainder: %.3f\n", delayTime, frameDurationRemainder);

            while ((Clock::GetTicks() - FrameTimeStart) < FrameTimeDesired);
        }
    }
    else {
        Clock::Delay(1);
    }
}
PUBLIC STATIC void Application::Run(int argc, char* args[]) {
    Application::Init(argc, args);
    if (!Running)
        return;

    Scene::Init();

    if (argc > 1) {
        char* pathStart = StringUtils::StrCaseStr(args[1], "/Resources/");
        if (pathStart == NULL)
            pathStart = StringUtils::StrCaseStr(args[1], "\\Resources\\");

        if (pathStart) {
            char* tmxPath = pathStart + strlen("/Resources/");
            for (char* i = tmxPath; *i; i++) {
                if (*i == '\\')
                    *i = '/';
            }
            Scene::LoadScene(tmxPath);
        }
        else {
            Log::Print(Log::LOG_WARN, "Map file \"%s\" not inside Resources folder!", args[1]);
        }
    }
    else if (*StartingScene) {
        Scene::LoadScene(StartingScene);
    }

    Scene::Restart();
    Application::UpdateWindowTitle();

    Graphics::Clear();
    Graphics::Present();

    #ifdef IOS
        // Initialize the Game Center for scoring and matchmaking
        // InitGameCenter();

        // Set up the game to run in the window animation callback on iOS
        // so that Game Center and so forth works correctly.
        SDL_iPhoneSetAnimationCallback(Application::Window, 1, RunFrame, NULL);
    #else
        while (Running) {
            if (BenchmarkFrameCount == 0)
                BenchmarkTickStart = Clock::GetTicks();

            Application::RunFrame(NULL);
            Application::DelayFrame();

            BenchmarkFrameCount++;
            if (BenchmarkFrameCount == TargetFPS) {
                double measuredSecond = Clock::GetTicks() - BenchmarkTickStart;
                FPS = 1000.0 / floor(measuredSecond) * TargetFPS;
                BenchmarkFrameCount = 0;
            }

            if (AutomaticPerformanceSnapshots && MetricFrameTime > AutomaticPerformanceSnapshotFrameTimeThreshold) {
                if (Clock::GetTicks() - AutomaticPerformanceSnapshotLastTime > AutomaticPerformanceSnapshotMinInterval) {
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

        if (DEBUG_fontSprite) {
            DEBUG_fontSprite->Dispose();
            delete DEBUG_fontSprite;
            DEBUG_fontSprite = NULL;
        }

        Application::Cleanup();

        Memory::PrintLeak();
    #endif
}

PRIVATE STATIC void Application::Cleanup() {
    ResourceManager::Dispose();
    AudioManager::Dispose();
    InputManager::Dispose();

    Graphics::Dispose();

    SDL_DestroyWindow(Application::Window);

    SDL_Quit();

    // Memory::PrintLeak();
}

PRIVATE STATIC void Application::LoadGameConfig() {
    StartingScene[0] = '\0';

    Application::GameConfig = XMLParser::ParseFromResource("GameConfig.xml");
    if (!Application::GameConfig) return;

    XMLNode* gameconfig = Application::GameConfig->children[0];

    for (size_t i = 0; i < gameconfig->children.size(); i++) {
        XMLNode* configItem = gameconfig->children[i];
        if (XMLParser::MatchToken(configItem->name, "startscene")) {
            Token value = configItem->children[0]->name;
            memcpy(StartingScene, value.Start, value.Length);
            StartingScene[value.Length] = 0;
        }
    }
}

PRIVATE STATIC void Application::DisposeGameConfig() {
    if (Application::GameConfig)
        XMLParser::Free(Application::GameConfig);

    Application::GameConfig = NULL;
}

PUBLIC STATIC void Application::LoadSettings() {
    if (Application::Settings)
        Application::Settings->Dispose();

    Application::Settings = INI::Load(Application::SettingsFile);

    // NOTE: If no settings could be loaded, create settings with default values.
    if (!Application::Settings) {
        Application::Settings = INI::New(Application::SettingsFile);

        Application::Settings->SetBool("display", "fullscreen", false);
        Application::Settings->SetBool("display", "vsync", false);
        Application::Settings->SetBool("display", "software", true);
    }

    int LogLevel = 0;
    #ifdef DEBUG
    LogLevel = -1;
    #endif
    #ifdef ANDROID
    LogLevel = -1;
    #endif
    Application::Settings->GetInteger("dev", "logLevel", &LogLevel);
    Application::Settings->GetBool("dev", "trackMemory", &Memory::IsTracking);
    Log::SetLogLevel(LogLevel);

    Application::Settings->GetBool("dev", "autoPerfSnapshots", &AutomaticPerformanceSnapshots);
    int apsFrameTimeThreshold = 20, apsMinInterval = 5;
    Application::Settings->GetInteger("dev", "apsMinFrameTime", &apsFrameTimeThreshold);
    Application::Settings->GetInteger("dev", "apsMinInterval", &apsMinInterval);
    AutomaticPerformanceSnapshotFrameTimeThreshold = apsFrameTimeThreshold;
    AutomaticPerformanceSnapshotMinInterval = apsMinInterval;

    Application::Settings->GetBool("display", "vsync", &Graphics::VsyncEnabled);
    Application::Settings->GetInteger("display", "multisample", &Graphics::MultisamplingEnabled);
}
PUBLIC STATIC void Application::SaveSettings() {
    if (Application::Settings)
        Application::Settings->Save();
}

PRIVATE STATIC int Application::HandleAppEvents(void* data, SDL_Event* event) {
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
