#ifndef ENGINE_FILESYSTEM_PATH_H
#define ENGINE_FILESYSTEM_PATH_H

#include <Engine/Includes/Standard.h>

#if WIN32
#define MAX_PATH_LENGTH 1024
#else
#define MAX_PATH_LENGTH 4096
#endif

enum PathLocation {
	// The default location.
	// This is usually the current directory, but it may not be
	// accessible depending on the system.
	DEFAULT,

	// The user location.
	// This is persistent storage and the location varies on the system.
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
	// In portable mode, this is the current directory.
	//
	// The equivalent URL is "user://"
	USER,

	// The saves location.
	// This is persistent storage and varies on the system.
	//
	// Typically, the path used is "user://saves/"
	//
	// The equivalent URL is "save://"
	SAVEGAME,

	// The preferences location.
	// This is local storage (non-roaming) and the location varies on the system.
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
	// In portable mode, this is the current directory.
	//
	// The equivalent URL is "config://"
	PREFERENCES,

	// The cache location.
	// This is local storage (non-roaming) and the location varies on the system.
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
	// In portable mode, this is under ".cache/" in the current directory.
	//
	// The equivalent URL is "cache://"
	CACHE
};

class Path {
private:
	static bool Create(const char* path);
	static bool GetCurrentWorkingDirectory(char* out, size_t sz);
	static bool MatchPaths(std::filesystem::path base, std::filesystem::path path);
	static PathLocation LocationFromURL(const char* filename);
	static std::filesystem::path GetCombinedPrefPath(const char* suffix);
#if LINUX
	static std::filesystem::path GetXdgPath(const char* xdg_env, const char* fallback_path);
#endif
	static std::filesystem::path GetGamePath();
	static std::filesystem::path GetBaseLocalPath();
	static std::filesystem::path GetBaseConfigPath();
	static std::filesystem::path GetCachePath();
	static std::filesystem::path GetForLocation(PathLocation location);
	static std::filesystem::path StripLocationFromURL(const char* filename, PathLocation& location);

public:
	static bool IsInDir(const char* dirPath, const char* path);
	static bool IsInCurrentDir(const char* path);
	static bool IsValidDefaultLocation(const char* filename);
	// static std::filesystem::path FromURLSimple(const char* filename, PathLocation& location);
	static bool FromURL(const char* filename, std::filesystem::path& result, PathLocation& location);
};

#endif /* ENGINE_FILESYSTEM_PATH_H */
