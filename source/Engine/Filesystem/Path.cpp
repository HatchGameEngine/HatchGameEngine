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
	std::make_pair("user://", PathLocation::USER),
	std::make_pair("save://", PathLocation::SAVEGAME),
	std::make_pair("config://", PathLocation::PREFERENCES),
	std::make_pair("cache://", PathLocation::CACHE)};

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

	std::string result = fsResult.u8string();

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

	if (!AreMatching(normBase.u8string(), finalPath.u8string())) {
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

	std::string result = fsPath.lexically_normal().u8string();

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

std::string Path::GetPrefPath() {
	if (Application::PortableMode) {
		return GetPortableModePath();
	}

	const char* devName = Application::GetDeveloperIdentifier();
	const char* gameName = Application::GetGameIdentifier();

	char* prefPath = SDL_GetPrefPath(devName, gameName);
	if (prefPath == nullptr) {
		return "";
	}

	std::string path = std::string(prefPath);

	SDL_free(prefPath);

	return path;
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
	std::string path;
	if (Application::PortableMode) {
		path = GetPortableModePath();
	}
	else {
		path = GetBaseUserPath();
	}

	if (path == "") {
		return "";
	}

	return Concat(path, suffix);
}

std::string Path::GetBaseConfigPath() {
	if (Application::PortableMode) {
		return GetPortableModePath();
	}

#if WIN32
	std::string gamePath = GetGameNamePath();
	if (gamePath == "") {
		return "";
	}

	std::string basePath;

	wchar_t* winPath = nullptr;
	if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &winPath) == S_OK) {
		std::filesystem::path fsPath = std::filesystem::path(winPath);

		basePath = fsPath.u8string();
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
#if WIN32
	// Returns %LocalAppData%
	return GetBaseConfigPath();
#elif LINUX
	if (Application::PortableMode) {
		return GetPortableModePath();
	}

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
	if (Application::PortableMode) {
		std::string workingDir = GetPortableModePath();
		if (workingDir == "") {
			return "";
		}

		return Concat(workingDir, "cache");
	}

#if WIN32
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

	if (finalPath == "") {
		return "";
	}

	// Ensure it ends in '/'
	if (finalPath.back() != '/') {
		finalPath += "/";
	}

	return finalPath;
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
	}

	if (location == PathLocation::DEFAULT) {
		return std::string(filename);
	}

	const char* pathAfterURL = filename + startingString.size();

	return std::string(pathAfterURL);
}

bool Path::IsValidDefaultLocation(const char* filename) {
	if (!IsInCurrentDir(filename)) {
		return false;
	}

	if (filename[0] == '/' || StringUtils::StartsWith(&filename[1], ":\\") ||
		StringUtils::StartsWith(&filename[1], ":/")) {
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
	std::string finalPath = combined.lexically_normal().u8string();

	if (finalPath.size() == 0) {
		return false;
	}

	// Set result
	result = finalPath;

	// Create the directories recursively if they don't exist.
	if (makeDirs && !Create(finalPath.c_str())) {
		return false;
	}

	if (!AreMatching(pathForLocationFs.u8string(), finalPath)) {
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

std::string Path::StripURL(const char* filename) {
	PathLocation location = PathLocation::DEFAULT;

	return StripLocationFromURL(filename, location);
}
