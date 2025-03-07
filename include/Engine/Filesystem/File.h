#ifndef ENGINE_FILESYSTEM_FILE_H
#define ENGINE_FILESYSTEM_FILE_H

#include <Engine/Includes/Standard.h>

class File {
public:
	static bool Exists(const char* path);
	static size_t ReadAllBytes(const char* path, char** out);
	static bool WriteAllBytes(const char* path, const char* bytes, size_t len);
};

#endif /* ENGINE_FILESYSTEM_FILE_H */
