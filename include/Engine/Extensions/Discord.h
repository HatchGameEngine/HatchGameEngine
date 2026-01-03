#ifndef ENGINE_EXTENSIONS_DISCORD_H
#define ENGINE_EXTENSIONS_DISCORD_H

#include <Engine/Includes/Standard.h>

struct IDiscordCore;
struct IDiscordActivityManager;

struct DiscordIntegrationActivity {
	const char* Details = nullptr;
	const char* State = nullptr;
	const char* LargeImageKey = nullptr;
	const char* LargeImageText = nullptr;
	const char* SmallImageKey = nullptr;
	const char* SmallImageText = nullptr;
	int PartySize = 0;
	int PartyMax = 0;
	time_t StartTime = 0;
	time_t EndTime = 0;
};

class Discord {
private:
	static struct IDiscordCore* Core;
	static struct IDiscordActivityManager* ActivityManager;

public:
	static bool Initialized;

	static void Init(const char* applicationID);
	static void Update();
	static void UpdatePresence(DiscordIntegrationActivity presence);
	static void Dispose();

	class Activity {
public:
		static void SetDetails(const char* details);
		static void SetState(const char* state);
		static void SetLargeImageKey(const char* key);
		static void SetLargeImageText(const char* text);
		static void SetLargeImage(const char* key, const char* text);
		static void SetSmallImageKey(const char* key);
		static void SetSmallImageText(const char* text);
		static void SetSmallImage(const char* key, const char* text);
		static void SetElapsedTimer(time_t timestamp);
		static void SetRemainingTimer(time_t timestamp);
		static void SetPartySize(int size);
		static void SetPartyMaxSize(int size);
		static void Update();
	};
};

#endif
