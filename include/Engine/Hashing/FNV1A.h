#ifndef ENGINE_HASHING_FNV1A_H
#define ENGINE_HASHING_FNV1A_H

#include <Engine/Includes/Standard.h>

namespace FNV1A {
//public:
	Uint32 EncryptString(char* message);
	Uint32 EncryptString(const char* message);
	Uint32 EncryptData(const void* data, size_t size);
	Uint32 EncryptData(const void* data, size_t size, Uint32 hash);
};

#endif /* ENGINE_HASHING_FNV1A_H */
