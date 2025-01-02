#ifndef STANDARDLIBS_H
#define STANDARDLIBS_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include <unordered_map>
#include <algorithm>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <deque>
#include <stack>
#include <filesystem>
#include <csetjmp>

#if WIN32
    #define snprintf _snprintf
    #undef __useHeader
    #undef __on_failure
#endif

#ifndef R_PI
    #define R_PI 3.1415927
#endif

template <typename T>
using vector = std::vector<T>;

template <typename T1, typename T2>
using map = std::map<T1, T2>;

template <typename T>
using stack = std::stack<T>;

template <typename T>
using deque = std::deque<T>;

using string = std::string;

enum class Platforms {
    Windows,
    MacOS,
    Linux,
    Switch,
    PlayStation,
    Xbox,
    Android,
    iOS,
    Unknown
};

enum class KeyBind {
    Fullscreen,
    DevRestartApp,
    DevRestartScene,
    DevRecompile,
    DevPerfSnapshot,
    DevLayerInfo,
    DevFastForward,
    DevFrameStepper,
    DevStepFrame,
    DevTileCol,
    DevObjectRegions,
    DevQuit,
    DevShowHitboxes,

    Max
};

enum {
    VIEWVAR_INVALID,
    VIEWVAR_BOOL,
    VIEWVAR_UINT8,
    VIEWVAR_UINT16,
    VIEWVAR_UINT32,
    VIEWVAR_INT8,
    VIEWVAR_INT16,
    VIEWVAR_INT32,
};

enum {
    VIEWVAR_DISPLAY_BOOL,
    VIEWVAR_DISPLAY_UNSIGNED,
    VIEWVAR_DISPLAY_SIGNED,
};

struct DeveloperMenu {
    void (*State)();
    int Selection;
    int SubSelection;
    int ScrollPos;
    int SubScrollPos;
    int Timer;
    bool Windowed;
    int SceneState;
    int ListPos;
    int WindowScale;
    int WindowAspect;
    bool ModsChanged;
    int PlayerListPos;
};
struct ViewableVariable {
    char Name[0x10];
    void* Value;
    int Type;
    int Size;
    int Min;
    int Max;
};

#define VIEWABLEVARIABLE_COUNT 64

#define DEFAULT_TARGET_FRAMERATE 60

#define MAX_TARGET_FRAMERATE 240

#define MAX_SCENE_VIEWS 8
#define MAX_PALETTE_COUNT 256
#define MAX_DEFORM_LINES 0x400
#define MAX_FRAMEBUFFER_HEIGHT 4096

#define SCOPE_SCENE 0
#define SCOPE_GAME 1

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;
typedef int64_t Sint64;

#ifdef IOS
#define NEW_STRUCT_MACRO(n) (n)
#else
#define NEW_STRUCT_MACRO(n) n
#endif

constexpr uint32_t MakeFourCC(const char *val) {
    return (val[0] << 24) | (val[1] << 16) | (val[2] << 8) | val[3];
}

#endif // STANDARDLIBS_H
