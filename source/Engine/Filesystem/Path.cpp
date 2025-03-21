#include <Engine/Application.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Utilities/StringUtils.h>

#if WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

std::pair<std::string, PathLocation> urlToLocationType[] = {
	std::make_pair("user://", PathLocation::USER),
	std::make_pair("save://", PathLocation::SAVEGAME),
	std::make_pair("config://", PathLocation::PREFERENCES),
	std::make_pair("cache://", PathLocation::CACHE)
};

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

#ifdef WIN32
	// Ensure path separators are '/' and not '\' on Windows
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

#ifdef WIN32
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

	GetCurrentWorkingDirectory(workingDir, sizeof workingDir);

	return IsInDir(workingDir, path);
}

bool Path::HasRelativeComponents(const char* path) {
	if (path == nullptr || path[0] == '\0') {
		return false;
	}

	std::string tempPath = std::string(path);

#ifdef WIN32
	std::replace(tempPath.begin(), tempPath.end(), '\\', '/');
#endif

	const char* component = tempPath.c_str();
	const char* found = strchr(component, '/');
	while (found != nullptr) {
		if (component[0] == '.'
		&& (component[1] == '/' || (component[1] == '.' && component[2] == '/'))) {
			return true;
		}

		component = found + 1;
		found = strchr(component, '/');
	}

	if (component[0] == '.'
	&& (component[1] == '/' || (component[1] == '.' && component[2] == '/'))) {
		return true;
	}

	return false;
}

std::string Path::Normalize(std::string path) {
	std::filesystem::path fsPath = std::filesystem::u8path(path);

	std::string result = fsPath.lexically_normal().u8string();

#ifdef WIN32
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

std::string Path::GetCombinedPrefPath(const char* suffix) {
	const char* devName = Application::GetDeveloperIdentifier();
	const char* gameName = Application::GetGameIdentifier();

	char* prefPath = SDL_GetPrefPath(devName, gameName);

	std::string path = std::string(prefPath);

	if (suffix != nullptr) {
		path = Concat(path, std::string(suffix));
	}

	SDL_free(prefPath);

	return path;
}

#if LINUX
std::string Path::GetXdgPath(const char* xdg_env, const char* fallback_path) {
	const char *env_path = SDL_getenv(xdg_env);
	if (!env_path) {
		env_path = SDL_getenv("HOME");
		if (env_path == nullptr) {
			return std::filesystem::u8path(std::string(""));
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

std::string Path::GetBaseLocalPath() {
	std::string gamePath = GetGameNamePath();
	if (gamePath == "") {
		return "";
	}

#if WIN32
	// TODO: Implement.
	std::string path = "";
#elif LINUX
	std::string path = GetXdgPath("XDG_DATA_HOME", ".local/share");
#else
	std::string path = "";
#endif

	if (path == "") {
		return path;
	}

	return Concat(path, gamePath);
}

std::string Path::GetBaseConfigPath() {
	std::string gamePath = GetGameNamePath();
	if (gamePath == "") {
		return "";
	}

#if WIN32
	// TODO: Implement.
	std::string path = "";
#elif LINUX
	std::string path = GetXdgPath("XDG_CONFIG_HOME", ".config");
#else
	std::string path = "";
#endif

	if (path == "") {
		return path;
	}

	return Concat(path, gamePath);
}

std::string Path::GetCachePath() {
	std::string gamePath = GetGameNamePath();
	if (gamePath == "") {
		return "";
	}

#if WIN32
	// TODO: Implement.
	std::string path = "";
#elif LINUX
	std::string path = GetXdgPath("XDG_CACHE_HOME", ".cache/");
#else
	std::string path = "";
#endif

	if (path == "") {
		return path;
	}

	return Concat(path, gamePath);
}

std::string Path::GetForLocation(PathLocation location) {
	std::string finalPath = "";

	const char* suffix = nullptr;

	switch (location) {
	case PathLocation::USER:
		finalPath = GetCombinedPrefPath(nullptr);
		break;
	case PathLocation::SAVEGAME:
		finalPath = GetCombinedPrefPath(Application::GetSavesDir());
		break;
	case PathLocation::PREFERENCES:
		finalPath = GetBaseConfigPath();
		if (finalPath != "") {
			suffix = Application::GetPreferencesDir();
			if (suffix != nullptr) {
				finalPath = Concat(finalPath, std::string(suffix));
			}
		}
		break;
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

#if 0
std::string Path::FromURLSimple(const char* filename, PathLocation& location) {
	std::string path = StripLocationFromURL(filename, location);
	if (location == PathLocation::DEFAULT) {
		return path;
	}

	std::string parentPath = GetForLocation(location);
	if (parentPath == "") {
		return "";
	}

	return Concat(parentPath, path);
}
#endif

bool Path::IsValidDefaultLocation(const char* filename) {
	if (!IsInCurrentDir(filename)) {
		return false;
	}

	if (filename[0] == '/'
	|| StringUtils::StartsWith(&filename[1], ":\\")
	|| StringUtils::StartsWith(&filename[1], ":/")) {
		return false;
	}

	return true;
}

bool Path::FromURL(const char* filename, std::string& result, PathLocation& location, bool makeDirs) {
	std::string detectedPath = StripLocationFromURL(filename, location);
	std::string finalPath = "";

	// Validate the path
	if (location == PathLocation::DEFAULT) {
		if (!IsValidDefaultLocation(detectedPath.c_str())) {
			return false;
		}

		finalPath = detectedPath;
	}
	else {
		// Attempt to resolve the path
		std::string pathForLocation = GetForLocation(location);
		if (pathForLocation == "") {
			return false;
		}

		// Create the directories recursively if they don't exist.
		if (makeDirs && !Create(pathForLocation.c_str())) {
			return false;
		}

		// Normalize both paths, and check for a mismatch
		std::filesystem::path pathForLocationFs = std::filesystem::u8path(pathForLocation);
		std::filesystem::path detectedPathFs = std::filesystem::u8path(detectedPath);

		pathForLocationFs = pathForLocationFs.lexically_normal();
		detectedPathFs = detectedPathFs.lexically_normal();

		std::filesystem::path combined = pathForLocationFs / detectedPathFs;
		finalPath = combined.lexically_normal().u8string();

		if (!AreMatching(pathForLocationFs.u8string(), finalPath)) {
			return false;
		}

#ifdef WIN32
		std::replace(finalPath.begin(), finalPath.end(), '\\', '/');
#endif
	}

	if (finalPath.size() == 0) {
		return false;
	}

	// '\' is not allowed (use '/' instead)
	if (strchr(finalPath.c_str(), '\\') != nullptr) {
		return false;
	}

	// Result path cannot be relative
	if (Path::HasRelativeComponents(finalPath.c_str())) {
		return false;
	}

	result = finalPath;

	return true;
}

std::string Path::StripURL(const char* filename) {
	PathLocation location = PathLocation::DEFAULT;

	return StripLocationFromURL(filename, location);
}
