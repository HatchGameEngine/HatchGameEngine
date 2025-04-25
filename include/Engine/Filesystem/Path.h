#ifndef ENGINE_FILESYSTEM_PATH_H
#define ENGINE_FILESYSTEM_PATH_H

#include <Engine/Includes/Standard.h>

#define MAX_PATH_LENGTH 4096
#define MAX_FILENAME_LENGTH 256

#define MAX_RESOURCE_PATH_LENGTH 1024

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

	// The logfile location.
	// This is local storage (non-roaming) and the location varies on the system.
	//
	// On Windows, this is the same as PREFERENCES.
	//
	// On Unix, this follows the XDG Base Directory specification.
	// Specifically, one of the following is used:
	// * $XDG_STATE_HOME/GameDeveloper/GameName/
	// * $XDG_STATE_HOME/GameName/
	//
	// In portable mode, this is the current directory.
	LOGFILE,

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
	// In portable mode, this is under "cache/" in the current directory.
	//
	// The equivalent URL is "cache://"
	CACHE
};

namespace Path {
//private:
	bool AreMatching(const std::string &base, const std::string &path);
	PathLocation LocationFromURL(const char* filename);
	std::string GetPortableModePath();
	std::string GetPrefPath();
	std::string GetFallbackLocalPath(const std::string &suffix);
#if LINUX
	std::string GetXdgPath(const char* xdg_env, const char* fallback_path);
#endif
	std::string GetGameNamePath();
	std::string GetBaseUserPath();
	std::string GetBaseConfigPath();
	std::string GetStatePath();
	std::string GetCachePath();
	std::string GetForLocation(PathLocation location);
	std::string StripLocationFromURL(const char* filename, PathLocation& location);
	bool ValidateForLocation(const char* path);

//public:
	bool Create(const char* path);
	std::string Concat(const std::string &pathA, const std::string &pathB);
	bool GetCurrentWorkingDirectory(char* out, size_t sz);
	bool IsInDir(const char* dirPath, const char* path);
	bool IsInCurrentDir(const char* path);
	bool HasRelativeComponents(const char* path);
	std::string Normalize(const std::string &path);
	std::string Normalize(const char* path);
	bool IsValidDefaultLocation(const char* filename);
	bool
	FromLocation(const std::string &path, PathLocation location, std::string& result, bool makeDirs);
	bool
	FromURL(const char* filename, std::string& result, PathLocation& location, bool makeDirs);
	std::string StripURL(const char* filename);
};

#endif /* ENGINE_FILESYSTEM_PATH_H */
