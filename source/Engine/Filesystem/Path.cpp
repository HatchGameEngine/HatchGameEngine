#include <Engine/Application.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Utilities/StringUtils.h>

#if WIN32
#include <direct.h>
#include <shlobj.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

std::pair<std::string, PathLocation> urlToLocationType[] = {
	std::make_pair(PATHLOCATION_GAME_URL, PathLocation::GAME),
	std::make_pair(PATHLOCATION_USER_URL, PathLocation::USER),
	std::make_pair(PATHLOCATION_SAVEGAME_URL, PathLocation::SAVEGAME),
	std::make_pair(PATHLOCATION_PREFERENCES_URL, PathLocation::PREFERENCES),
	std::make_pair(PATHLOCATION_CACHE_URL, PathLocation::CACHE)};

std::string Path::ToString(std::filesystem::path path) {
#if __cplusplus >= 202002L
	std::u8string string = path.u8string();

	return std::string(string.begin(), string.end());
#else
	return path.u8string();
#endif
}

// Creates a path recursively.
// The path MUST end with '/' if it's meant to create a path to a folder.
bool Path::Create(const char* path) {
	if (path == nullptr || path[0] == '\0') {
		return false;
	}

	char* buffer = StringUtils::Create(path);
	if (buffer == nullptr) {
		return false;
	}

#if WIN32
	// Ensure path separators are '/' on Windows
	StringUtils::ReplacePathSeparatorsInPlace(buffer);
#endif

	// If path is absolute, start from the second character
	// as if the path were relative
	char* bufferPtr = buffer;
	if (*bufferPtr == '/') {
		bufferPtr++;
	}

	// Remove any potential filename from the path
	char* ptr = strrchr(bufferPtr, '/');
	if (ptr) {
		ptr[1] = '\0';
	}
	else {
		// No folder afterwards, must be at the root "/"
		return true;
	}

	ptr = strchr(bufferPtr, '/');
	while (ptr != nullptr) {
		*ptr = '\0';

		if (!Directory::Create(buffer)) {
			Memory::Free(buffer);
			return false;
		}

		*ptr = '/';
		ptr = strchr(ptr + 1, '/');
	}

	bool success = Directory::Create(buffer);

	Memory::Free(buffer);

	return success;
}

std::string Path::Concat(std::string pathA, std::string pathB) {
	std::filesystem::path fsPathA = std::filesystem::u8path(pathA);
	std::filesystem::path fsPathB = std::filesystem::u8path(pathB);

	std::filesystem::path fsResult = fsPathA / fsPathB;

	std::string result = ToString(fsResult);

#if WIN32
	std::replace(result.begin(), result.end(), '\\', '/');
#endif

	return result;
}

bool Path::GetCurrentWorkingDirectory(char* out, size_t sz) {
#if WIN32
	return _getcwd(out, sz) != NULL;
#else
	return getcwd(out, sz) != NULL;
#endif
}

std::string Path::GetPortableModePath() {
	char workingDir[MAX_PATH_LENGTH];
	if (!GetCurrentWorkingDirectory(workingDir, sizeof workingDir)) {
		return "";
	}

	return std::string(workingDir);
}

bool Path::AreMatching(std::string base, std::string path) {
	auto const last = std::prev(base.end());

	return std::mismatch(base.begin(), last, path.begin()).first == last;
}

bool Path::IsInDir(const char* dirPath, const char* path) {
	if (dirPath == nullptr || path == nullptr) {
		return false;
	}

	std::filesystem::path basePath = std::filesystem::u8path(std::string(dirPath));
	std::filesystem::path fsPath = std::filesystem::u8path(std::string(path));

	std::filesystem::path normBase = basePath.lexically_normal();
	std::filesystem::path finalPath = (normBase / fsPath).lexically_normal();

	if (!AreMatching(ToString(normBase), ToString(finalPath))) {
		return false;
	}

	return true;
}

bool Path::IsInCurrentDir(const char* path) {
	char workingDir[MAX_PATH_LENGTH];
	if (!GetCurrentWorkingDirectory(workingDir, sizeof workingDir)) {
		return false;
	}

	return IsInDir(workingDir, path);
}

bool Path::HasRelativeComponents(const char* path) {
	if (path == nullptr || path[0] == '\0') {
		return false;
	}

	std::string tempPath = std::string(path);

#if WIN32
	std::replace(tempPath.begin(), tempPath.end(), '\\', '/');
#endif

	const char* component = tempPath.c_str();
	const char* found = strchr(component, '/');
	while (found != nullptr) {
		if (component[0] == '.' &&
			(component[1] == '/' || (component[1] == '.' && component[2] == '/'))) {
			return true;
		}

		component = found + 1;
		found = strchr(component, '/');
	}

	if (component[0] == '.' &&
		(component[1] == '/' || (component[1] == '.' && component[2] == '/'))) {
		return true;
	}

	return false;
}

std::string Path::Normalize(std::string path) {
	std::filesystem::path fsPath = std::filesystem::u8path(path);

	std::string result = ToString(fsPath.lexically_normal());

#if WIN32
	std::replace(result.begin(), result.end(), '\\', '/');
#endif

	return result;
}
std::string Path::Normalize(const char* path) {
	return Normalize(std::string(path));
}

PathLocation Path::LocationFromURL(const char* filename) {
	for (std::pair<std::string, PathLocation> pair : urlToLocationType) {
		if (StringUtils::StartsWith(filename, pair.first.c_str())) {
			return pair.second;
		}
	}

	return PathLocation::DEFAULT;
}

#ifdef CONSOLE_FILESYSTEM
std::string Path::GetConsoleBasePath() {
#ifdef CONSOLE_BASE_PATH
	// Compile-time defined base path
	constexpr std::string_view basePath = CONSOLE_BASE_PATH;
	// Safety check for base path macro
	static_assert(!basePath.empty() && basePath.back() == '/', "Path must end with '/'");
	return std::string(basePath);
#elif defined(SWITCH_ROMFS)
	return std::string("romfs:/");
#else
	// Fallback path is cwd, add a custom path if needed (ending with '/')
	return GetPortableModePath();
#endif
}

std::string Path::GetConsolePrefPath() {
#if defined(SWITCH)
	// "/GAME_NAME/" (root of the SD card)
	std::string gameName = Application::GetGameIdentifier();
	return std::string("/" + gameName + "/");
#else
	// Fallback path is cwd, add a custom path if needed (ending with '/')
	return GetPortableModePath();
#endif
}
#endif

std::string Path::GetBasePath() {
#ifdef CONSOLE_FILESYSTEM
	return GetConsoleBasePath();
#else
	char* basePath = SDL_GetBasePath();
	if (basePath == nullptr) {
		return "";
	}

	std::string path = std::string(basePath);

	SDL_free(basePath);

	return path;
#endif
}

std::string Path::GetPrefPath() {
#ifdef CONSOLE_FILESYSTEM
	return GetConsolePrefPath();
#elif defined(PORTABLE_MODE)
	return GetPortableModePath();
#else
	const char* devName = Application::GetDeveloperIdentifier();
	const char* gameName = Application::GetGameIdentifier();

	char* prefPath = SDL_GetPrefPath(devName, gameName);
	if (prefPath == nullptr) {
		return "";
	}

	std::string path = std::string(prefPath);

	SDL_free(prefPath);

	return path;
#endif
}

#if LINUX
std::string Path::GetXdgPath(const char* xdg_env, const char* fallback_path) {
	const char* env_path = SDL_getenv(xdg_env);
	if (!env_path) {
		env_path = SDL_getenv("HOME");
		if (env_path == nullptr) {
			return "";
		}

		return Concat(env_path, fallback_path);
	}

	return std::string(env_path);
}
#endif

std::string Path::GetGameNamePath() {
	const char* gameName = Application::GetGameIdentifier();
	if (gameName == nullptr) {
		return "";
	}

	const char* devName = Application::GetDeveloperIdentifier();
	if (devName == nullptr) {
		return std::string(gameName);
	}

	return Concat(std::string(devName), std::string(gameName));
}

std::string Path::GetBaseUserPath() {
	return GetPrefPath();
}

std::string Path::GetFallbackLocalPath(std::string suffix) {
#ifdef PORTABLE_MODE
	std::string path = GetPortableModePath();
#else
	std::string path = GetBaseUserPath();
#endif

	if (path == "") {
		return "";
	}

	return Concat(path, suffix);
}

std::string Path::GetBaseConfigPath() {
#ifdef PORTABLE_MODE
	return GetPortableModePath();
#elif WIN32
	std::string gamePath = GetGameNamePath();
	if (gamePath == "") {
		return "";
	}

	std::string basePath;

	wchar_t* winPath = nullptr;
	if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &winPath) == S_OK) {
		std::filesystem::path fsPath = std::filesystem::path(winPath);

		basePath = ToString(fsPath);
		std::replace(basePath.begin(), basePath.end(), '\\', '/');

		CoTaskMemFree(winPath);
	}
	else {
		return "";
	}

	return Concat(basePath, gamePath);
#elif LINUX
	std::string gamePath = GetGameNamePath();
	if (gamePath == "") {
		return "";
	}

	std::string xdgPath = GetXdgPath("XDG_CONFIG_HOME", ".config");
	if (xdgPath == "") {
		return "";
	}

	return Concat(xdgPath, gamePath);
#else
	return GetFallbackLocalPath("config");
#endif
}

std::string Path::GetStatePath() {
#ifdef PORTABLE_MODE
	return GetPortableModePath();
#elif WIN32
	// Returns %LocalAppData%
	return GetBaseConfigPath();
#elif LINUX
	std::string gamePath = GetGameNamePath();
	if (gamePath == "") {
		return "";
	}

	std::string xdgPath = GetXdgPath("XDG_STATE_HOME", ".local/state");
	if (xdgPath == "") {
		return "";
	}

	return Concat(xdgPath, gamePath);
#else
	return GetFallbackLocalPath("local");
#endif
}

std::string Path::GetCachePath() {
#ifdef PORTABLE_MODE
	std::string workingDir = GetPortableModePath();
	if (workingDir == "") {
		return "";
	}

	return Concat(workingDir, "cache");
#elif WIN32
	// Returns %LocalAppData%
	return GetBaseConfigPath();
#elif LINUX
	std::string gamePath = GetGameNamePath();
	if (gamePath == "") {
		return "";
	}

	std::string xdgPath = GetXdgPath("XDG_CACHE_HOME", ".cache");
	if (xdgPath == "") {
		return "";
	}

	return Concat(xdgPath, gamePath);
#else
	return GetFallbackLocalPath("cache");
#endif
}

std::string Path::GetForLocation(PathLocation location) {
	std::string finalPath = "";

	const char* suffix = nullptr;

	switch (location) {
	// game://
	case PathLocation::GAME:
		finalPath = GetBasePath();
		break;
	// user://
	case PathLocation::USER:
		finalPath = GetBaseUserPath();
		break;
	// save://
	case PathLocation::SAVEGAME:
		finalPath = GetBaseUserPath();
		if (finalPath != "") {
			suffix = Application::GetSavesDir();
			if (suffix != nullptr) {
				finalPath = Concat(finalPath, std::string(suffix));
			}
		}
		break;
	// config://
	case PathLocation::PREFERENCES:
		finalPath = GetBaseConfigPath();
		if (finalPath != "") {
			suffix = Application::GetPreferencesDir();
			if (suffix != nullptr) {
				finalPath = Concat(finalPath, std::string(suffix));
			}
		}
		break;
	// Log file location (no URL)
	case PathLocation::LOGFILE:
		finalPath = GetStatePath();
		break;
	// cache://
	case PathLocation::CACHE:
		finalPath = GetCachePath();
		break;
	default:
		break;
	}

// FIXME: If using root "/" as current working directory, this fails to find files
#ifndef CONSOLE_FILESYSTEM
	if (finalPath == "") {
		return "";
	}
#endif

	// Ensure it ends in '/'
	if (finalPath.back() != '/') {
		finalPath += "/";
	}

	return finalPath;
}

std::string Path::GetLocationFromRealPath(const char* filename, PathLocation location) {
	std::string pathForLocation = GetForLocation(location);
	if (pathForLocation == "") {
		return "";
	}

	std::filesystem::path locFs = std::filesystem::u8path(pathForLocation);
	std::filesystem::path pathFs = std::filesystem::u8path(std::string(filename));

	locFs = locFs.lexically_normal();
	pathFs = pathFs.lexically_normal();

	if (locFs == pathFs) {
		return "";
	}

	if (!StringUtils::StartsWith(pathFs.c_str(), locFs.c_str())) {
		return "";
	}

	return pathFs.u8string().substr(locFs.u8string().size());
}

std::string Path::StripLocationFromURL(const char* filename, PathLocation& location) {
	std::string startingString = "";

	location = PathLocation::DEFAULT;

	for (std::pair<std::string, PathLocation> pair : urlToLocationType) {
		std::string pathStart = pair.first;

		if (StringUtils::StartsWith(filename, pathStart.c_str())) {
			startingString = pathStart;
			location = pair.second;
			break;
		}
		else {
			std::string fromRealPath = GetLocationFromRealPath(filename, pair.second);

			if (fromRealPath != "") {
				location = pair.second;
				return fromRealPath;
			}
		}
	}

	if (location == PathLocation::DEFAULT) {
		return std::string(filename);
	}

	const char* pathAfterURL = filename + startingString.size();

	return std::string(pathAfterURL);
}

bool Path::IsAbsolute(const char* filename) {
	if (filename[0] == '/' || StringUtils::StartsWith(&filename[1], ":\\") ||
		StringUtils::StartsWith(&filename[1], ":/")) {
		return true;
	}

	return false;
}

bool Path::IsValidDefaultLocation(const char* filename) {
	if (!IsInCurrentDir(filename)) {
		return false;
	}

	if (Path::IsAbsolute(filename)) {
		return false;
	}

	return true;
}

bool Path::ValidateForLocation(const char* path) {
	// '\' is not allowed. Use '/' instead.
	if (strchr(path, '\\') != nullptr) {
		return false;
	}

	// Result path cannot be relative
	if (Path::HasRelativeComponents(path)) {
		return false;
	}

	// Cannot have '://'
	if (StringUtils::StrCaseStr(path, "://")) {
		return false;
	}

	return true;
}

bool Path::FromLocation(std::string path,
	PathLocation location,
	std::string& result,
	bool makeDirs) {
	if (location == PathLocation::DEFAULT) {
		// Validate the path
		if (path.size() == 0) {
			return false;
		}

		// Set result
		result = path;

		const char* pathStr = path.c_str();
		if (!IsValidDefaultLocation(pathStr)) {
			return false;
		}

		return ValidateForLocation(pathStr);
	}

	// Attempt to resolve the path
	std::string pathForLocation = GetForLocation(location);
	if (pathForLocation == "") {
		return false;
	}

	// Normalize both paths, and check for a mismatch
	std::filesystem::path pathForLocationFs = std::filesystem::u8path(pathForLocation);
	std::filesystem::path detectedPathFs = std::filesystem::u8path(path);

	pathForLocationFs = pathForLocationFs.lexically_normal();
	detectedPathFs = detectedPathFs.lexically_normal();

	std::filesystem::path combined = pathForLocationFs / detectedPathFs;
	std::string finalPath = ToString(combined.lexically_normal());

	if (finalPath.size() == 0) {
		return false;
	}

	// Set result
	result = finalPath;

	// Create the directories recursively if they don't exist.
	if (makeDirs && !Create(finalPath.c_str())) {
		return false;
	}

	if (!AreMatching(ToString(pathForLocationFs), finalPath)) {
		return false;
	}

#if WIN32
	std::replace(finalPath.begin(), finalPath.end(), '\\', '/');
#endif

	return ValidateForLocation(finalPath.c_str());
}

bool Path::FromURL(const char* filename,
	std::string& result,
	PathLocation& location,
	bool makeDirs) {
	std::string detectedPath = StripLocationFromURL(filename, location);

	return FromLocation(detectedPath, location, result, makeDirs);
}

bool Path::FromURL(const char* filename, std::string& result) {
	PathLocation location;

	std::string detectedPath = StripLocationFromURL(filename, location);

	return FromLocation(detectedPath, location, result, false);
}

void Path::FromURL(const char* filename, char* buf, size_t bufSize) {
	if (buf == nullptr) {
		return;
	}

	std::string resolved = "";
	if (Path::FromURL(filename, resolved)) {
		StringUtils::Copy(buf, resolved.c_str(), bufSize);
	}
	else {
		buf[0] = '\0';
	}
}

std::string Path::StripURL(const char* filename) {
	PathLocation location = PathLocation::DEFAULT;

	return StripLocationFromURL(filename, location);
}
