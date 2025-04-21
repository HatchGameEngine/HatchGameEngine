#include <Engine/Diagnostics/Memory.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Utilities/StringUtils.h>

#if WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

bool CompareFunction(std::filesystem::path i, std::filesystem::path j) {
	std::string pathA = i.string();
	std::string pathB = j.string();

	return pathA.compare(pathB) < 0;
}

bool Directory::Exists(const char* path) {
#if WIN32
	DWORD ftyp = GetFileAttributesA(path);
	if (ftyp == INVALID_FILE_ATTRIBUTES) {
		return false; // Something is wrong with your path
	}
	if (ftyp & FILE_ATTRIBUTE_DIRECTORY) {
		return true;
	}
#else
	DIR* dir = opendir(path);
	if (dir) {
		closedir(dir);
		return true;
	}
#endif
	return false;
}
bool Directory::Create(const char* path) {
	if (path[0] == '\0') {
		return false;
	}

#if WIN32
	BOOL result = CreateDirectoryA(path, NULL);
	if (result == FALSE) {
		DWORD err = GetLastError();
		if (err == ERROR_ALREADY_EXISTS) {
			return true;
		}

		return false;
	}
#else
	int result = mkdir(path, 0700);
	if (result != 0) {
		if (errno == EEXIST) {
			return true;
		}

		return false;
	}
#endif

	return true;
}

void Directory::GetFiles(std::vector<std::filesystem::path>* files,
	const char* path,
	const char* searchPattern,
	bool allDirs) {
#if WIN32
	char winPath[MAX_PATH_LENGTH];
	snprintf(winPath, MAX_PATH_LENGTH, "%s%s*", path, path[strlen(path) - 1] == '/' ? "" : "/");

	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFile(winPath, &data);

	int i;
	char fullpath[MAX_PATH_LENGTH];
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (data.cFileName[0] == '.' && data.cFileName[1] == 0) {
				continue;
			}
			if (data.cFileName[0] == '.' && data.cFileName[1] == '.' &&
				data.cFileName[2] == 0) {
				continue;
			}

			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (allDirs) {
					snprintf(fullpath,
						sizeof fullpath,
						"%s/%s",
						path,
						data.cFileName);
					Directory::GetFiles(files, fullpath, searchPattern, true);
				}
			}
			else if (StringUtils::WildcardMatch(data.cFileName, searchPattern)) {
				std::string entryName = std::string(data.cFileName);

				std::filesystem::path pathFs =
					std::filesystem::path(std::string(path));
				std::filesystem::path entryFs = std::filesystem::path(entryName);

				std::string tempPath = (pathFs / entryFs).string();
				std::replace(tempPath.begin(), tempPath.end(), '\\', '/');

				std::filesystem::path finalPath = std::filesystem::u8path(tempPath);

				files->push_back(finalPath);
			}
		} while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
#else
	char fullpath[MAX_PATH_LENGTH];
	DIR* dir = opendir(path);
	if (dir) {
		size_t i;
		struct dirent* d;

		while ((d = readdir(dir)) != NULL) {
			if (d->d_name[0] == '.' && !d->d_name[1]) {
				continue;
			}
			if (d->d_name[0] == '.' && d->d_name[1] == '.' && !d->d_name[2]) {
				continue;
			}

			if (d->d_type == DT_DIR) {
				if (allDirs) {
					snprintf(fullpath,
						sizeof fullpath,
						"%s/%s",
						path,
						d->d_name);
					Directory::GetFiles(files, fullpath, searchPattern, true);
				}
			}
			else if (StringUtils::WildcardMatch(d->d_name, searchPattern)) {
				std::string entryName = std::string(d->d_name);

				std::filesystem::path pathFs =
					std::filesystem::u8path(std::string(path));
				std::filesystem::path entryFs = std::filesystem::u8path(entryName);

				files->push_back(pathFs / entryFs);
			}
		}
		closedir(dir);
	}
#endif

	std::sort(files->begin(), files->end(), CompareFunction);
}
std::vector<std::filesystem::path>
Directory::GetFiles(const char* path, const char* searchPattern, bool allDirs) {
	std::vector<std::filesystem::path> files;
	Directory::GetFiles(&files, path, searchPattern, allDirs);
	return files;
}

void Directory::GetDirectories(std::vector<std::filesystem::path>* files,
	const char* path,
	const char* searchPattern,
	bool allDirs) {
#if WIN32
	char winPath[MAX_PATH_LENGTH];
	snprintf(winPath, MAX_PATH_LENGTH, "%s%s*", path, path[strlen(path) - 1] == '/' ? "" : "/");

	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFile(winPath, &data);

	int i;
	char fullpath[MAX_PATH_LENGTH];
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (data.cFileName[0] == '.' && !data.cFileName[1]) {
				continue;
			}
			if (data.cFileName[0] == '.' && data.cFileName[1] == '.' &&
				!data.cFileName[2]) {
				continue;
			}

			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (StringUtils::WildcardMatch(data.cFileName, searchPattern)) {
					std::string entryName = std::string(data.cFileName);

					std::filesystem::path pathFs =
						std::filesystem::path(std::string(path));
					std::filesystem::path entryFs =
						std::filesystem::path(entryName);

					std::string tempPath = (pathFs / entryFs).string();
					std::replace(tempPath.begin(), tempPath.end(), '\\', '/');

					std::filesystem::path finalPath =
						std::filesystem::u8path(tempPath);

					files->push_back(finalPath);
				}
				if (allDirs) {
					snprintf(fullpath,
						sizeof fullpath,
						"%s/%s",
						path,
						data.cFileName);
					Directory::GetFiles(files, fullpath, searchPattern, true);
				}
			}
		} while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
#else
	char fullpath[MAX_PATH_LENGTH];
	DIR* dir = opendir(path);
	if (dir) {
		int i;
		struct dirent* d;

		while ((d = readdir(dir)) != NULL) {
			if (d->d_name[0] == '.' && !d->d_name[1]) {
				continue;
			}
			if (d->d_name[0] == '.' && d->d_name[1] == '.' && !d->d_name[2]) {
				continue;
			}

			if (d->d_type == DT_DIR) {
				if (StringUtils::WildcardMatch(d->d_name, searchPattern)) {
					std::string entryName = std::string(d->d_name);

					std::filesystem::path pathFs =
						std::filesystem::u8path(std::string(path));
					std::filesystem::path entryFs =
						std::filesystem::u8path(entryName);

					files->push_back(pathFs / entryFs);
				}
				if (allDirs) {
					snprintf(fullpath,
						sizeof fullpath,
						"%s/%s",
						path,
						d->d_name);
					Directory::GetFiles(files, fullpath, searchPattern, true);
				}
			}
		}
		closedir(dir);
	}
#endif

	std::sort(files->begin(), files->end(), CompareFunction);
}
std::vector<std::filesystem::path>
Directory::GetDirectories(const char* path, const char* searchPattern, bool allDirs) {
	std::vector<std::filesystem::path> files;
	Directory::GetDirectories(&files, path, searchPattern, allDirs);
	return files;
}
