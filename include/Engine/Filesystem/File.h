#ifndef ENGINE_FILESYSTEM_FILE_H
#define ENGINE_FILESYSTEM_FILE_H

#include <Engine/Includes/Standard.h>

namespace File {
//public:
	bool Exists(const char* path, bool allowURLs);
	bool Exists(const char* path);
	size_t ReadAllBytes(const char* path, char** out, bool allowURLs);
	bool WriteAllBytes(const char* path, const char* bytes, size_t len, bool allowURLs);
};

#endif /* ENGINE_FILESYSTEM_FILE_H */
