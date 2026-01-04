#ifndef ENGINE_EXTENSIONS_DISCORD_H
#define ENGINE_EXTENSIONS_DISCORD_H

#include <Engine/Includes/Standard.h>

struct IDiscordCore;
struct IDiscordActivityManager;

class Discord {
public:
    static bool Initialized;
    static struct IDiscordCore* Core;
    static struct IDiscordActivityManager* ActivityManager;

    static void Init(const char* applicationID);
    static void Update();
    static void UpdatePresence(const char* details);
    static void UpdatePresence(const char* details, const char* state);
    static void UpdatePresence(const char* details, const char* state, const char* largeImageKey);
    static void UpdatePresence(const char* details, const char* state, const char* largeImageKey, const char* smallImageKey);
    static void UpdatePresence(const char* details, const char* state, const char* largeImageKey, time_t startTime);
    static void UpdatePresence(const char* details, const char* state, const char* largeImageKey, const char* smallImageKey, time_t startTime);
    static void UpdatePresence(const char* details, const char* state, const char* largeImageKey, int partySize, int partyMax);
    static void UpdatePresence(const char* details, const char* state, const char* largeImageKey, const char* smallImageKey, int partySize, int partyMax);
    static void UpdatePresence(const char* details, const char* state, const char* largeImageKey, const char* smallImageKey, int partySize, int partyMax, time_t startTime);
    static void Dispose();
};

#endif