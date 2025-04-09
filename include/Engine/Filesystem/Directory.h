#ifndef ENGINE_FILESYSTEM_DIRECTORY_H
#define ENGINE_FILESYSTEM_DIRECTORY_H

#include <Engine/Includes/Standard.h>

namespace Directory {
//public:
	bool Exists(const char* path);
	bool Create(const char* path);
	void GetFiles(std::vector<std::filesystem::path>* files,
		const char* path,
		const char* searchPattern,
		bool allDirs);
	std::vector<std::filesystem::path>
	GetFiles(const char* path, const char* searchPattern, bool allDirs);
	void GetDirectories(std::vector<std::filesystem::path>* files,
		const char* path,
		const char* searchPattern,
		bool allDirs);
	std::vector<std::filesystem::path>
	GetDirectories(const char* path, const char* searchPattern, bool allDirs);
};

#endif /* ENGINE_FILESYSTEM_DIRECTORY_H */
