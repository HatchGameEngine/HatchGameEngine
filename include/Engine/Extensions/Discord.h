#ifndef ENGINE_EXTENSIONS_DISCORD_H
#define ENGINE_EXTENSIONS_DISCORD_H

#include <Engine/Includes/Standard.h>

class Discord {
public:
	static bool Initialized;

	static void Init(const char* application_id, const char* steam_id);
	static void UpdatePresence(char* details);
	static void UpdatePresence(char* details, char* state);
	static void UpdatePresence(char* details, char* state, char* image_key);
	static void UpdatePresence(char* details, char* state, char* image_key, time_t start_time);
	static void
	UpdatePresence(char* details, char* state, char* image_key, int party_size, int party_max);
	static void UpdatePresence(char* details,
		char* state,
		char* image_key,
		int party_size,
		int party_max,
		time_t start_time);
	static void Dispose();
};

#endif /* ENGINE_EXTENSIONS_DISCORD_H */
