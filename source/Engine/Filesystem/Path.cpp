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

// Creates a path recursively
bool Path::Create(const char* path) {
	char* tempPath = StringUtils::Duplicate(path);
	char* work = tempPath;
	if (work == nullptr) {
		return false;
	}

	if (work[0] == '/')
		work++;

	char* ptr = strrchr(work, '/');
	if (ptr) {
		ptr[1] = '\0';
	}

	ptr = strchr(work, '/');
	while (ptr != nullptr) {
		*ptr = '\0';

		if (!Directory::Create(tempPath)) {
			Memory::Free(tempPath);
			return false;
		}

		*ptr = '/';
		ptr = strchr(ptr + 1, '/');
	}

	bool success = Directory::Create(tempPath);

	Memory::Free(tempPath);

	return success;
}

bool Path::GetCurrentWorkingDirectory(char* out, size_t sz) {
#if WIN32
	return _getcwd(out, sz) != NULL;
#else
	return getcwd(out, sz) != NULL;
#endif
}

bool Path::MatchPaths(std::filesystem::path base, std::filesystem::path path) {
	auto const last = std::prev(base.end());

	return std::mismatch(base.begin(), last, path.begin()).first == last;
}

bool Path::IsInDir(const char* dirPath, const char* path) {
	if (dirPath == nullptr || path == nullptr) {
		return false;
	}

	std::filesystem::path basePath = std::filesystem::path(std::string(dirPath));
	std::filesystem::path fsPath = std::filesystem::path(std::string(path));

	std::filesystem::path normBase = basePath.lexically_normal();
	std::filesystem::path finalPath = (normBase / fsPath).lexically_normal();

	if (!MatchPaths(normBase, finalPath)) {
		return false;
	}

	return true;
}

bool Path::IsInCurrentDir(const char* path) {
	char workingDir[MAX_PATH_LENGTH];

	GetCurrentWorkingDirectory(workingDir, sizeof workingDir);

	return IsInDir(workingDir, path);
}

PathLocation Path::LocationFromURL(const char* filename) {
	for (std::pair<std::string, PathLocation> pair : urlToLocationType) {
		if (StringUtils::StartsWith(filename, pair.first.c_str())) {
			return pair.second;
		}
	}

	return PathLocation::DEFAULT;
}

std::filesystem::path Path::GetCombinedPrefPath(const char* suffix) {
	const char* devName = Application::GetDeveloperIdentifier();
	const char* gameName = Application::GetGameIdentifier();

	std::filesystem::path path = "";

	char* prefPath = SDL_GetPrefPath(devName, gameName);
	path = std::filesystem::path(std::string(prefPath));
	if (suffix != nullptr) {
		path = path / (std::string(suffix) + "/");
	}
	SDL_free(prefPath);

	return path;
}

#if LINUX
std::filesystem::path Path::GetXdgPath(const char* xdg_env, const char* fallback_path) {
	const char *env_path = SDL_getenv(xdg_env);
	if (!env_path) {
		env_path = SDL_getenv("HOME");
		if (env_path == nullptr) {
			return std::filesystem::path(std::string(""));
		}

		return std::filesystem::path(std::string(env_path)) / std::filesystem::path(std::string(fallback_path));
	}
	else {
		return std::filesystem::path(std::string(env_path));
	}
}
#endif

std::filesystem::path Path::GetGamePath() {
	const char* devName = Application::GetDeveloperIdentifier();
	const char* gameName = Application::GetGameIdentifier();

	if (gameName == nullptr) {
		return "";
	}

	std::filesystem::path path = "";

	if (devName != nullptr) {
		path = path / std::filesystem::path(std::string(devName) + "/");
	}

	return path / std::filesystem::path(std::string(gameName) + "/");
}

std::filesystem::path Path::GetBaseLocalPath() {
	std::filesystem::path gamePath = GetGamePath();
	if (gamePath == "") {
		return gamePath;
	}

#if WIN32
	// TODO: Implement.
	std::filesystem::path path = "";
#elif LINUX
	std::filesystem::path path = GetXdgPath("XDG_DATA_HOME", ".local/share");
#else
	std::filesystem::path path = "";
#endif

	if (path == "") {
		return path;
	}

	return path / gamePath;
}

std::filesystem::path Path::GetBaseConfigPath() {
	std::filesystem::path gamePath = GetGamePath();
	if (gamePath == "") {
		return gamePath;
	}

#if WIN32
	// TODO: Implement.
	std::filesystem::path path = "";
#elif LINUX
	std::filesystem::path path = GetXdgPath("XDG_CONFIG_HOME", ".config");
#else
	std::filesystem::path path = "";
#endif

	if (path == "") {
		return path;
	}

	return path / gamePath;
}

std::filesystem::path Path::GetCachePath() {
	std::filesystem::path gamePath = GetGamePath();
	if (gamePath == "") {
		return gamePath;
	}

#if WIN32
	// TODO: Implement.
	std::filesystem::path path = "";
#elif LINUX
	std::filesystem::path path = GetXdgPath("XDG_CACHE_HOME", ".cache/");
#else
	std::filesystem::path path = "";
#endif

	if (path == "") {
		return path;
	}

	return path / gamePath;
}

std::filesystem::path Path::GetForLocation(PathLocation location) {
	std::filesystem::path finalPath = "";

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
				finalPath = finalPath / std::string(suffix);
			}
		}
		break;
	case PathLocation::CACHE:
		finalPath = GetCachePath();
		break;
	default:
		return "";
	}

	return finalPath;
}

std::filesystem::path Path::StripLocationFromURL(const char* filename, PathLocation& location) {
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

	std::filesystem::path result = "";

	if (location == PathLocation::DEFAULT) {
		result = std::string(filename);
	}
	else {
		const char* pathAfterURL = filename + startingString.size();

		result = std::string(pathAfterURL);
	}

	return std::filesystem::path(result);
}

#if 0
std::filesystem::path Path::FromURLSimple(const char* filename, PathLocation& location) {
	std::filesystem::path path = StripLocationFromURL(filename, location);
	if (location == PathLocation::DEFAULT) {
		return path;
	}

	std::filesystem::path parentPath = GetForLocation(location);
	if (parentPath == "") {
		return parentPath;
	}

	return parentPath / path;
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

bool Path::FromURL(const char* filename, std::filesystem::path& result, PathLocation& location) {
	std::filesystem::path detectedPath = StripLocationFromURL(filename, location);

	// Validate the path
	if (location == PathLocation::DEFAULT) {
		result = detectedPath;

		if (!IsValidDefaultLocation(detectedPath.c_str())) {
			return false;
		}
	}
	else {
		// Attempt to resolve the path
		std::filesystem::path pathForLocation = GetForLocation(location);
		if (pathForLocation == "") {
			return false;
		}

		// Create the directories recursively if they don't exist.
		std::string pathForLocationString = pathForLocation.u8string();
		if (!Create(pathForLocationString.c_str())) {
			return false;
		}

		std::filesystem::path normBase = pathForLocation.lexically_normal();

		result = (normBase / detectedPath).lexically_normal();

		if (!MatchPaths(normBase, result)) {
			return false;
		}
	}

	return true;
}

std::filesystem::path Path::StripURL(const char* filename) {
	PathLocation location = PathLocation::DEFAULT;

	return StripLocationFromURL(filename, location);
}
