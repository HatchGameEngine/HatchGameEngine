#ifndef ENGINE_HASHING_MURMUR_H
#define ENGINE_HASHING_MURMUR_H

#include <Engine/Includes/Standard.h>

namespace Murmur {
//public:
	Uint32 EncryptString(char* message);
	Uint32 EncryptString(const char* message);
	Uint32 EncryptData(const void* data, size_t size);
	Uint32 EncryptData(const void* key, size_t size, Uint32 hash);
};

#endif /* ENGINE_HASHING_MURMUR_H */
