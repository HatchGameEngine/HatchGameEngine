#ifndef ENGINE_EXTENSIONS_DISCORD_H
#define ENGINE_EXTENSIONS_DISCORD_H

#include <Engine/Includes/Standard.h>

namespace Discord {
//public:
	extern bool Initialized;

	void Init(const char* application_id, const char* steam_id);
	void UpdatePresence(char* details);
	void UpdatePresence(char* details, char* state);
	void UpdatePresence(char* details, char* state, char* image_key);
	void UpdatePresence(char* details, char* state, char* image_key, time_t start_time);
	void
	UpdatePresence(char* details, char* state, char* image_key, int party_size, int party_max);
	void UpdatePresence(char* details,
		char* state,
		char* image_key,
		int party_size,
		int party_max,
		time_t start_time);
	void Dispose();
};

#endif /* ENGINE_EXTENSIONS_DISCORD_H */
