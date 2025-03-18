#ifndef ENGINE_FILESYSTEM_DIRECTORY_H
#define ENGINE_FILESYSTEM_DIRECTORY_H

#include <Engine/Includes/Standard.h>

class Directory {
public:
	static bool Exists(const char* path);
	static bool Create(const char* path);
	static void
	GetFiles(vector<char*>* files, const char* path, const char* searchPattern, bool allDirs);
	static vector<char*> GetFiles(const char* path, const char* searchPattern, bool allDirs);
	static void GetDirectories(vector<char*>* files,
		const char* path,
		const char* searchPattern,
		bool allDirs);
	static vector<char*>
	GetDirectories(const char* path, const char* searchPattern, bool allDirs);
};

#endif /* ENGINE_FILESYSTEM_DIRECTORY_H */
