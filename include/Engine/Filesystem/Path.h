#ifndef ENGINE_FILESYSTEM_PATH_H
#define ENGINE_FILESYSTEM_PATH_H

#include <Engine/Includes/Standard.h>

#define MAX_PATH_LENGTH 4096
#define MAX_FILENAME_LENGTH 256

#define MAX_RESOURCE_PATH_LENGTH 1024

#define PATHLOCATION_GAME_URL "game://"
#define PATHLOCATION_USER_URL "user://"
#define PATHLOCATION_SAVEGAME_URL "save://"
#define PATHLOCATION_PREFERENCES_URL "config://"
#define PATHLOCATION_CACHE_URL "cache://"

enum PathLocation {
	// The default location.
	// This is usually the current directory, but it may not be
	// accessible depending on the system.
	DEFAULT,

	// The location of game data.
	// The path varies on the system, and may be the current directory.
	//
	// The equivalent URL is "game://"
	GAME,

	// The user location.
	// This is persistent storage, and the path varies on the system.
	//
	// On Windows, this is one of:
	// * C:/Users/username/AppData/Roaming/GameDeveloper/GameName/
	// * C:/Users/username/AppData/Roaming/GameName/
	//
	// On Unix, this follows the XDG Base Directory specification.
	// Specifically, one of the following is used:
	// * $XDG_DATA_HOME/GameDeveloper/GameName/
	// * $XDG_DATA_HOME/GameName/
	//
	// The equivalent URL is "user://"
	USER,

	// The saves location.
	// This is persistent storage, and the path varies on the system.
	//
	// Typically, the path used is "user://saves/"
	//
	// The equivalent URL is "save://"
	SAVEGAME,

	// The preferences location.
	// This is non-roaming storage, and the path varies on the system.
	//
	// On Windows, this is one of:
	// * C:/Users/username/AppData/Local/GameDeveloper/GameName/
	// * C:/Users/username/AppData/Local/GameName/
	//
	// On Unix, this follows the XDG Base Directory specification.
	// Specifically, one of the following is used:
	// * $XDG_CONFIG_HOME/GameDeveloper/GameName/
	// * $XDG_CONFIG_HOME/GameName/
	//
	// The equivalent URL is "config://"
	PREFERENCES,

	// The logfile location.
	// This is non-roaming storage, and the path varies on the system.
	//
	// On Windows, this is the same as PREFERENCES.
	//
	// On Unix, this follows the XDG Base Directory specification.
	// Specifically, one of the following is used:
	// * $XDG_STATE_HOME/GameDeveloper/GameName/
	// * $XDG_STATE_HOME/GameName/
	LOGFILE,

	// The cache location.
	// This is non-roaming storage, and the path varies on the system.
	//
	// On Windows, this is one of:
	// * C:/Users/username/AppData/Local/GameDeveloper/GameName/cache/
	// * C:/Users/username/AppData/Local/GameName/cache/
	//
	// On Unix, this follows the XDG Base Directory specification.
	// Specifically, one of the following is used:
	// * $XDG_CACHE_HOME/GameDeveloper/GameName/
	// * $XDG_CACHE_HOME/GameName/
	//
	// The equivalent URL is "cache://"
	CACHE
};

class Path {
private:
	static bool AreMatching(std::string base, std::string path);
	static PathLocation LocationFromURL(const char* filename);
	static std::string GetPortableModePath();
#ifdef CONSOLE_FILESYSTEM
	static std::string GetConsoleBasePath();
	static std::string GetConsolePrefPath();
#endif
	static std::string GetBasePath();
	static std::string GetPrefPath();
	static std::string GetFallbackLocalPath(std::string suffix);
#if LINUX
	static std::string GetXdgPath(const char* xdg_env, const char* fallback_path);
#endif
	static std::string GetGameNamePath();
	static std::string GetBaseUserPath();
	static std::string GetBaseConfigPath();
	static std::string GetStatePath();
	static std::string GetCachePath();
	static std::string GetForLocation(PathLocation location);
	static std::string StripLocationFromURL(const char* filename, PathLocation& location);
	static bool ValidateForLocation(const char* path);

public:
	static std::string ToString(std::filesystem::path path);
	static bool Create(const char* path);
	static std::string Concat(std::string pathA, std::string pathB);
	static bool GetCurrentWorkingDirectory(char* out, size_t sz);
	static bool IsInDir(const char* dirPath, const char* path);
	static bool IsInCurrentDir(const char* path);
	static bool HasRelativeComponents(const char* path);
	static std::string Normalize(std::string path);
	static std::string Normalize(const char* path);
	static bool IsValidDefaultLocation(const char* filename);
	static bool
	FromLocation(std::string path, PathLocation location, std::string& result, bool makeDirs);
	static bool
	FromURL(const char* filename, std::string& result, PathLocation& location, bool makeDirs);
	static bool FromURL(const char* filename, std::string& result);
	static std::string StripURL(const char* filename);
};

#endif /* ENGINE_FILESYSTEM_PATH_H */
