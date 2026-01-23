#ifndef ENGINE_EXTENSIONS_DISCORD_H
#define ENGINE_EXTENSIONS_DISCORD_H

#include <Engine/Includes/Standard.h>

// This is a subset of EDiscordResult
enum {
    DISCORDRESULT_OK,
    DISCORDRESULT_ERROR,
    DISCORDRESULT_SERVICEUNAVAILABLE,
    DISCORDRESULT_INVALIDVERSION,
    DISCORDRESULT_INVALIDPAYLOAD,
    DISCORDRESULT_INVALIDPERMISSIONS,
    DISCORDRESULT_NOTFOUND,
    DISCORDRESULT_NOTAUTHENTICATED,
    DISCORDRESULT_NOTINSTALLED,
    DISCORDRESULT_NOTRUNNING,
    DISCORDRESULT_RATELIMITED
};

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

typedef void (*DiscordIntegrationCallbackFuncPtr)(void* data);

enum DiscordIntegrationCallbackType {
	DiscordIntegrationCallbackType_FuncPtr,
	DiscordIntegrationCallbackType_Script
};

struct DiscordIntegrationCallback {
	DiscordIntegrationCallbackType Type;
	void* Function;
};

struct DiscordIntegrationUserAvatar {
	unsigned Width;
	unsigned Height;
	Uint8* Data;
	char Identifier[256];
};

struct DiscordIntegrationUserInfo {
	Sint64 IDSnowflake;
	char ID[32];
	char Username[256];
	bool IsBot;
	DiscordIntegrationUserAvatar Avatar;
};

class Discord {
private:
	static void Unload();

public:
	static bool Initialized;

	static int Init(const char* applicationID);
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

	class User {
	public:
		static int Update();
		static bool IsUserPresent();
		static DiscordIntegrationUserInfo* GetDetails();
		static void GetAvatar(DiscordIntegrationUserAvatar* avatar,
			int size,
			DiscordIntegrationCallback* callback);
		static void GetAvatar(DiscordIntegrationUserAvatar* avatar,
			DiscordIntegrationCallback* callback);
		static void GetAvatar(int size, DiscordIntegrationCallback* callback);
	};
};

#endif
