#ifndef STANDARDLIBS_H
#define STANDARDLIBS_H

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <csetjmp>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include <Engine/Includes/Endian.h>

#if WIN32
#define snprintf _snprintf
#undef __useHeader
#undef __on_failure
#endif

template<typename T>
using vector = std::vector<T>;

template<typename T1, typename T2>
using map = std::map<T1, T2>;

template<typename T>
using stack = std::stack<T>;

template<typename T>
using deque = std::deque<T>;

using string = std::string;

enum class Platforms { Windows, MacOS, Linux, Switch, PlayStation, Xbox, Android, iOS, Unknown };

enum class KeyBind {
	Fullscreen,
	ToggleFPSCounter,

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
	DevMenuToggle,
	DevQuit,

	Max
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
};

#define DEFAULT_TARGET_FRAMERATE 60

#define MAX_TARGET_FRAMERATE 240

#define MATRIX_STACK_SIZE 256
#define FILTER_TABLE_SIZE 0x8000
#define PALETTE_ROW_SIZE 0x100

#define MAX_SCENE_VIEWS 8
#define MAX_PALETTE_COUNT 256
#define MAX_DEFORM_LINES 0x400
#define MAX_FRAMEBUFFER_HEIGHT 4096

#define SCOPE_SCENE 0
#define SCOPE_GAME 1

#define PALETTE_INDEX_TABLE_ID -1

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;
typedef int64_t Sint64;

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif

#ifndef M_PI_HALF
#define M_PI_HALF (M_PI / 2)
#endif

#define RSDK_PI 3.1415927

#ifdef IOS
#define NEW_STRUCT_MACRO(n) (n)
#else
#define NEW_STRUCT_MACRO(n) n
#endif

#endif // STANDARDLIBS_H
