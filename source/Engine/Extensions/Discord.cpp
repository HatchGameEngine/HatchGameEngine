#include <Engine/Application.h>
#include <Engine/Extensions/Discord.h>

#if defined(WIN32) || defined(LINUX)
#include <Engine/Extensions/discord_game_sdk.h>

#ifdef WIN32
#define DISCORD_LIBRARY_NAME "discord_game_sdk.dll"
#else
#define DISCORD_LIBRARY_NAME "discord_game_sdk.so"
#endif

typedef enum EDiscordResult(DISCORD_API* DiscordCreate_t)(DiscordVersion version,
	struct DiscordCreateParams* params,
	struct IDiscordCore** result);

void* library = NULL;
DiscordCreate_t _DiscordCreate = NULL;

bool Discord::Initialized = false;
struct IDiscordCore* Discord::Core = NULL;
struct IDiscordActivityManager* Discord::ActivityManager = NULL;

struct DiscordActivity CurrentActivity;

void DISCORD_CALLBACK OnUpdateActivityCallback(void* callback_data, enum EDiscordResult result) {
	if (result == DiscordResult_Ok) {
		Log::Print(Log::LOG_API, "Discord: Presence updated successfully.");
	}
	else {
		Log::Print(
			Log::LOG_API, "Discord: Failed to update presence (Error %d)", (int)result);
	}
}

void Discord::Init(const char* applicationID) {
	if (Discord::Initialized) {
		return;
	}
	Discord::Initialized = false;

	if (!library) {
		library = SDL_LoadObject(DISCORD_LIBRARY_NAME);
		if (!library) {
			Log::Print(Log::LOG_API,
				"Discord: Failed to load %s! (SDL Error: %s)",
				DISCORD_LIBRARY_NAME,
				SDL_GetError());
			return;
		}
	}

	if (!_DiscordCreate) {
		_DiscordCreate = (DiscordCreate_t)SDL_LoadFunction(library, "DiscordCreate");
		if (!_DiscordCreate) {
			Log::Print(Log::LOG_API,
				"Discord: Failed to find function 'DiscordCreate' in %s!",
				DISCORD_LIBRARY_NAME);
			SDL_UnloadObject(library);
			return;
		}
	}

	struct DiscordCreateParams params;
	DiscordCreateParamsSetDefault(&params);
	params.client_id = strtoll(applicationID, NULL, 10);
	params.flags = DiscordCreateFlags_NoRequireDiscord;

	if (Discord::Core) {
		Discord::Core->destroy(Discord::Core);
		Discord::Core = NULL;
	}

	enum EDiscordResult result = _DiscordCreate(DISCORD_VERSION, &params, &Discord::Core);
	if (result != DiscordResult_Ok) {
		if (result == DiscordResult_InternalError || result == DiscordResult_NotRunning) {
			Log::Print(Log::LOG_API,
				"Discord: Integration disabled (Discord client not found).");
		}
		else {
			Log::Print(Log::LOG_API,
				"Discord: Initialization failed with unexpected error code %d",
				(int)result);
		}

		SDL_UnloadObject(library);
		library = NULL;
		return;
	}

	Discord::ActivityManager = Discord::Core->get_activity_manager(Discord::Core);
	Discord::Initialized = true;
	Log::Print(Log::LOG_API,
		"Discord: SDK initialized successfully for Application ID: %s",
		applicationID);

	memset(&CurrentActivity, 0, sizeof(CurrentActivity));
	CurrentActivity.type = DiscordActivityType_Playing;
}

void Discord::Update() {
	if (!Discord::Initialized) {
		return;
	}

	Discord::Core->run_callbacks(Discord::Core);
}

void Discord::SetDetails(const char* details) {
	if (details && strlen(details) > 0) {
		strncpy(CurrentActivity.details, details, sizeof(CurrentActivity.details) - 1);
	}
	else {
		memset(CurrentActivity.details, 0, sizeof(CurrentActivity.details) - 1);
	}
}
void Discord::SetState(const char* state) {
	if (state && strlen(state) > 0) {
		strncpy(CurrentActivity.state, state, sizeof(CurrentActivity.state) - 1);
	}
	else {
		memset(CurrentActivity.state, 0, sizeof(CurrentActivity.state) - 1);
	}
}
void Discord::SetLargeImageKey(const char* key) {
	if (key && strlen(key) > 0) {
		strncpy(CurrentActivity.assets.large_image,
			key,
			sizeof(CurrentActivity.assets.large_image) - 1);
	}
	else {
		memset(CurrentActivity.assets.large_image,
			0,
			sizeof(CurrentActivity.assets.large_image) - 1);
	}
}
void Discord::SetLargeImageText(const char* text) {
	if (text && strlen(text) > 0) {
		strncpy(CurrentActivity.assets.large_text,
			text,
			sizeof(CurrentActivity.assets.large_text) - 1);
	}
	else {
		memset(CurrentActivity.assets.large_text,
			0,
			sizeof(CurrentActivity.assets.large_text) - 1);
	}
}
void Discord::SetLargeImage(const char* key, const char* text) {
	Discord::SetLargeImageKey(key);
	Discord::SetLargeImageText(text);
}
void Discord::SetSmallImageKey(const char* key) {
	if (key && strlen(key) > 0) {
		strncpy(CurrentActivity.assets.small_image,
			key,
			sizeof(CurrentActivity.assets.small_image) - 1);
	}
	else {
		memset(CurrentActivity.assets.small_image,
			0,
			sizeof(CurrentActivity.assets.small_image) - 1);
	}
}
void Discord::SetSmallImageText(const char* text) {
	if (text && strlen(text) > 0) {
		strncpy(CurrentActivity.assets.small_text,
			text,
			sizeof(CurrentActivity.assets.small_text) - 1);
	}
	else {
		memset(CurrentActivity.assets.small_text,
			0,
			sizeof(CurrentActivity.assets.small_text) - 1);
	}
}
void Discord::SetSmallImage(const char* key, const char* text) {
	Discord::SetSmallImageKey(key);
	Discord::SetSmallImageText(text);
}

void Discord::SetStartTime(time_t startTime) {
	CurrentActivity.timestamps.start = (DiscordTimestamp)startTime;
	CurrentActivity.timestamps.end = 0;
}
void Discord::SetEndTime(time_t endTime) {
	CurrentActivity.timestamps.start = 0;
	CurrentActivity.timestamps.end = (DiscordTimestamp)endTime;
}
void Discord::SetPartySize(int size) {
	CurrentActivity.party.size.current_size = size;
}
void Discord::SetPartyMaxSize(int size) {
	CurrentActivity.party.size.max_size = size;
}

void Discord::UpdateActivity() {
	if (!Discord::Initialized || !Discord::ActivityManager) {
		return;
	}

	Discord::ActivityManager->update_activity(
		Discord::ActivityManager, &CurrentActivity, NULL, OnUpdateActivityCallback);
}

void Discord::UpdatePresence(struct DiscordIntegrationActivity presence) {
	if (!Discord::Initialized || !Discord::ActivityManager) {
		return;
	}

	Discord::SetDetails(presence.Details);
	Discord::SetState(presence.State);
	Discord::SetLargeImage(presence.LargeImageKey, presence.LargeImageText);
	Discord::SetSmallImage(presence.SmallImageKey, presence.SmallImageText);

	if (presence.EndTime) {
		Discord::SetEndTime(presence.EndTime);
	}
	else {
		Discord::SetStartTime(presence.StartTime);
	}

	Discord::SetPartySize(presence.PartySize);
	Discord::SetPartyMaxSize(presence.PartyMax);

	Discord::UpdateActivity();
}

void Discord::Dispose() {
	if (!Discord::Initialized) {
		return;
	}

	if (Discord::Core) {
		Discord::Core->destroy(Discord::Core);
		Discord::Core = NULL;
	}

	SDL_UnloadObject(library);
	Discord::Initialized = false;
}

#else
bool Discord::Initialized = false;
struct IDiscordCore* Discord::Core = NULL;
struct IDiscordActivityManager* Discord::ActivityManager = NULL;

void Discord::Init(const char* application_id) {
	Discord::Initialized = false;
}
void Discord::Update() {}

void Discord::SetDetails(const char* details) {}
void Discord::SetState(const char* state) {}
void Discord::SetLargeImageKey(const char* key) {}
void Discord::SetLargeImageText(const char* text) {}
void Discord::SetLargeImage(const char* key, const char* text) {}
void Discord::SetSmallImageKey(const char* key) {}
void Discord::SetSmallImageText(const char* text) {}
void Discord::SetSmallImage(const char* key, const char* text) {}

void Discord::SetStartTime(time_t startTime) {}
void Discord::SetEndTime(time_t endTime) {}
void Discord::SetPartySize(int size) {}
void Discord::SetPartyMaxSize(int size) {}

void Discord::UpdateActivity() {}
void Discord::UpdatePresence(struct DiscordPresence presence) {}
void Discord::Dispose() {}

#endif
