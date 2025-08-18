#ifndef ENGINE_FILESYSTEM_FILE_H
#define ENGINE_FILESYSTEM_FILE_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>

class File {
public:
	enum { READ_ACCESS, WRITE_ACCESS, APPEND_ACCESS };

	static Stream* Open(const char* filename, Uint32 access);
	static bool Exists(const char* path);
	static bool ProtectedExists(const char* path, bool allowURLs);
	static size_t ReadAllBytes(const char* path, char** out, bool allowURLs);
	static bool WriteAllBytes(const char* path, const char* bytes, size_t len, bool allowURLs);
};

#endif /* ENGINE_FILESYSTEM_FILE_H */
