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

void Discord::Activity::SetDetails(const char* details) {
	if (details && strlen(details) > 0) {
		strncpy(CurrentActivity.details, details, sizeof(CurrentActivity.details) - 1);
	}
	else {
		memset(CurrentActivity.details, 0, sizeof(CurrentActivity.details) - 1);
	}
}
void Discord::Activity::SetState(const char* state) {
	if (state && strlen(state) > 0) {
		strncpy(CurrentActivity.state, state, sizeof(CurrentActivity.state) - 1);
	}
	else {
		memset(CurrentActivity.state, 0, sizeof(CurrentActivity.state) - 1);
	}
}
void Discord::Activity::SetLargeImageKey(const char* key) {
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
void Discord::Activity::SetLargeImageText(const char* text) {
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
void Discord::Activity::SetLargeImage(const char* key, const char* text) {
	Discord::Activity::SetLargeImageKey(key);
	Discord::Activity::SetLargeImageText(text);
}
void Discord::Activity::SetSmallImageKey(const char* key) {
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
void Discord::Activity::SetSmallImageText(const char* text) {
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
void Discord::Activity::SetSmallImage(const char* key, const char* text) {
	Discord::Activity::SetSmallImageKey(key);
	Discord::Activity::SetSmallImageText(text);
}

void Discord::Activity::SetElapsedTimer(time_t timestamp) {
	CurrentActivity.timestamps.start = (DiscordTimestamp)timestamp;
	CurrentActivity.timestamps.end = 0;
}
void Discord::Activity::SetRemainingTimer(time_t timestamp) {
	CurrentActivity.timestamps.start = 0;
	CurrentActivity.timestamps.end = (DiscordTimestamp)timestamp;
}
void Discord::Activity::SetPartySize(int size) {
	CurrentActivity.party.size.current_size = size;
}
void Discord::Activity::SetPartyMaxSize(int size) {
	CurrentActivity.party.size.max_size = size;
}

void Discord::Activity::Update() {
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

	Discord::Activity::SetDetails(presence.Details);
	Discord::Activity::SetState(presence.State);
	Discord::Activity::SetLargeImage(presence.LargeImageKey, presence.LargeImageText);
	Discord::Activity::SetSmallImage(presence.SmallImageKey, presence.SmallImageText);

	if (presence.EndTime) {
		Discord::Activity::SetRemainingTimer(presence.EndTime);
	}
	else {
		Discord::Activity::SetElapsedTimer(presence.StartTime);
	}

	Discord::Activity::SetPartySize(presence.PartySize);
	Discord::Activity::SetPartyMaxSize(presence.PartyMax);

	Discord::Activity::Update();
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

void Discord::Activity::SetDetails(const char* details) {}
void Discord::Activity::SetState(const char* state) {}
void Discord::Activity::SetLargeImageKey(const char* key) {}
void Discord::Activity::SetLargeImageText(const char* text) {}
void Discord::Activity::SetLargeImage(const char* key, const char* text) {}
void Discord::Activity::SetSmallImageKey(const char* key) {}
void Discord::Activity::SetSmallImageText(const char* text) {}
void Discord::Activity::SetSmallImage(const char* key, const char* text) {}

void Discord::Activity::SetElapsedTimer(time_t timestamp) {}
void Discord::Activity::SetRemainingTimer(time_t timestamp) {}
void Discord::Activity::SetPartySize(int size) {}
void Discord::Activity::SetPartyMaxSize(int size) {}

void Discord::Activity::Update() {}

void Discord::UpdatePresence(struct DiscordPresence presence) {}
void Discord::Dispose() {}

#endif
