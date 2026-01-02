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

    static void Init(const char* application_id);
    static void Update();
    static void UpdatePresence(char* details);
    static void UpdatePresence(char* details, char* state);
    static void UpdatePresence(char* details, char* state, char* imageKey);
    static void UpdatePresence(char* details, char* state, char* imageKey, time_t start_time);
    static void UpdatePresence(char* details, char* state, char* imageKey, int partySize, int partyMax);
    static void UpdatePresence(char* details, char* state, char* imageKey, int partySize, int partyMax, time_t start_time);
    static void Dispose();
};

#endif