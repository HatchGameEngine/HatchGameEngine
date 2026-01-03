#include <Engine/Application.h>
#include <Engine/Extensions/Discord.h>

#if defined(WIN32) || defined(LINUX)
#include <Engine/Extensions/discord_game_sdk.h>

#ifdef WIN32
#define DISCORD_LIBRARY_NAME "discord_game_sdk.dll"
#else
#define DISCORD_LIBRARY_NAME "discord_game_sdk.so"
#endif

typedef enum EDiscordResult(DISCORD_API* DiscordCreate_t)(DiscordVersion version, struct DiscordCreateParams* params, struct IDiscordCore** result);

void* library = NULL;
DiscordCreate_t _DiscordCreate = NULL;

bool Discord::Initialized = false;
struct IDiscordCore* Discord::Core = NULL;
struct IDiscordActivityManager* Discord::ActivityManager = NULL;

void DISCORD_CALLBACK OnUpdateActivityCallback(void* callback_data, enum EDiscordResult result) {
    if (result == DiscordResult_Ok) {
        Log::Print(Log::LOG_API, "Discord: Presence updated successfully.");
    }
    else {
        Log::Print(Log::LOG_API, "Discord: Failed to update presence (Error %d)", (int)result);
    }
}

void Discord::Init(const char* applicationID) {
    Discord::Initialized = false;

    library = SDL_LoadObject(DISCORD_LIBRARY_NAME);
    if (!library) {
        Log::Print(Log::LOG_API, "Discord: Failed to load %s! (SDL Error: %s)", DISCORD_LIBRARY_NAME, SDL_GetError());
        return;
    }

    _DiscordCreate = (DiscordCreate_t)SDL_LoadFunction(library, "DiscordCreate");
    if (!_DiscordCreate) {
        Log::Print(Log::LOG_API, "Discord: Failed to find function 'DiscordCreate' in %s!", DISCORD_LIBRARY_NAME);
        SDL_UnloadObject(library);
        return;
    }

    struct DiscordCreateParams params;
    DiscordCreateParamsSetDefault(&params);
    params.client_id = strtoll(applicationID, NULL, 10);
    params.flags = DiscordCreateFlags_NoRequireDiscord;

    enum EDiscordResult result = _DiscordCreate(DISCORD_VERSION, &params, &Discord::Core);
    if (result != DiscordResult_Ok) {
        if (result == DiscordResult_InternalError || result == DiscordResult_NotRunning) {
            Log::Print(Log::LOG_API, "Discord: Integration disabled (Discord client not found).");
        }
        else {
            Log::Print(Log::LOG_API, "Discord: Initialization failed with unexpected error code %d", (int)result);
        }

        SDL_UnloadObject(library);
        library = NULL;
        return;
    }

    Discord::ActivityManager = Discord::Core->get_activity_manager(Discord::Core);
    Discord::Initialized = true;
    Log::Print(Log::LOG_API, "Discord: SDK initialized successfully for Application ID: %s", applicationID);
}

void Discord::Update() {
    if (!Discord::Initialized) return;

    Discord::Core->run_callbacks(Discord::Core);
}

void Discord::UpdatePresence(const char* details, const char* state, const char* imageKey, int partySize, int partyMax, time_t startTime) {
    if (!Discord::Initialized || !Discord::ActivityManager) return;

    struct DiscordActivity activity;
    memset(&activity, 0, sizeof(activity));

    if (details && strlen(details) > 0)
        strncpy(activity.details, details, sizeof(activity.details) - 1);

    if (state && strlen(state) > 0)
        strncpy(activity.state, state, sizeof(activity.state) - 1);

    if (imageKey && strlen(imageKey) > 0)
        strncpy(activity.assets.large_image, imageKey, sizeof(activity.assets.large_image) - 1);

    activity.timestamps.start = (DiscordTimestamp)startTime;
    activity.party.size.current_size = partySize;
    activity.party.size.max_size = partyMax;
    activity.type = DiscordActivityType_Playing;

    Discord::ActivityManager->update_activity(Discord::ActivityManager, &activity, NULL, OnUpdateActivityCallback);
}

void Discord::Dispose() {
    if (!Discord::Initialized) return;

    Discord::Core->destroy(Discord::Core);
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
void Discord::UpdatePresence(const char* details, const char* state, const char* imageKey, int partySize, int partyMax, time_t startTime) {}
void Discord::Dispose() {}

#endif

void Discord::UpdatePresence(const char* details) { UpdatePresence(details, NULL, NULL, 0, 0, 0); }

void Discord::UpdatePresence(const char* details, const char* state) { UpdatePresence(details, state, NULL, 0, 0, 0); }

void Discord::UpdatePresence(const char* details, const char* state, const char* imageKey) { UpdatePresence(details, state, imageKey, 0, 0, 0); }

void Discord::UpdatePresence(const char* details, const char* state, const char* imageKey, time_t startTime) { UpdatePresence(details, state, imageKey, 0, 0, startTime); }

void Discord::UpdatePresence(const char* details, const char* state, const char* imageKey, int partySize, int partyMax) { UpdatePresence(details, state, imageKey, partySize, partyMax, 0); }