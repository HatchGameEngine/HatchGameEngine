#include <Engine/Application.h>
#include <Engine/Extensions/Discord.h>

#ifdef WIN32

#include <discord_rpc.h>

#define HAS_DISCORD_RPC

void* library = NULL;
void (*_Discord_Initialize)(const char*, DiscordEventHandlers*, int, const char*) = NULL;
void (*_Discord_UpdatePresence)(const DiscordRichPresence*) = NULL;
void (*_Discord_ClearPresence)(void) = NULL;
void (*_Discord_Shutdown)(void) = NULL;

#endif

bool Discord::Initialized = false;

void Discord::Init(const char* application_id, const char* steam_id) {
	Discord::Initialized = false;

#ifdef HAS_DISCORD_RPC
	library = SDL_LoadObject("discord-rpc.dll");
	if (!library) {
		return;
	}

	_Discord_Initialize =
		(void (*)(const char*, DiscordEventHandlers*, int, const char*))SDL_LoadFunction(
			library, "Discord_Initialize");
	_Discord_UpdatePresence = (void (*)(const DiscordRichPresence*))SDL_LoadFunction(
		library, "Discord_UpdatePresence");
	_Discord_ClearPresence = (void (*)(void))SDL_LoadFunction(library, "Discord_ClearPresence");
	_Discord_Shutdown = (void (*)(void))SDL_LoadFunction(library, "Discord_Shutdown");

	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));

	_Discord_Initialize(application_id, &handlers, 1, steam_id);

	Discord::Initialized = true;
	Discord::UpdatePresence(NULL);
#endif
}

void Discord::UpdatePresence(char* details) {
	Discord::UpdatePresence(details, NULL);
}
void Discord::UpdatePresence(char* details, char* state) {
	Discord::UpdatePresence(details, state, NULL);
}
void Discord::UpdatePresence(char* details, char* state, char* image_key) {
	Discord::UpdatePresence(details, state, image_key, 0, 0);
}
void Discord::UpdatePresence(char* details, char* state, char* image_key, time_t start_time) {
	Discord::UpdatePresence(details, state, image_key, 0, 0, start_time);
}
void Discord::UpdatePresence(char* details,
	char* state,
	char* image_key,
	int party_size,
	int party_max) {
	Discord::UpdatePresence(details, state, image_key, party_size, party_max, 0);
}
void Discord::UpdatePresence(char* details,
	char* state,
	char* image_key,
	int party_size,
	int party_max,
	time_t start_time) {
	if (!Discord::Initialized) {
		return;
	}

#ifdef HAS_DISCORD_RPC
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));

	discordPresence.details = details;
	discordPresence.state = state;
	discordPresence.largeImageKey = image_key;
	discordPresence.partySize = party_size;
	discordPresence.partyMax = party_max;
	discordPresence.startTimestamp = start_time;
	discordPresence.instance = 1;
	_Discord_UpdatePresence(&discordPresence);
#endif
}

void Discord::Dispose() {
	if (!Discord::Initialized) {
		return;
	}

#ifdef HAS_DISCORD_RPC
	_Discord_Shutdown();
	SDL_UnloadObject(library);
#endif
}
