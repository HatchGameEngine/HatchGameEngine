#ifndef ENGINE_HASHING_CRC32_H
#define ENGINE_HASHING_CRC32_H

#include <Engine/Includes/Standard.h>

namespace CRC32 {
//public:
	Uint32 EncryptString(char* data);
	Uint32 EncryptString(const char* message);
	Uint32 EncryptData(const void* data, size_t size);
	Uint32 EncryptData(const void* data, size_t size, Uint32 crc);
};

#endif /* ENGINE_HASHING_CRC32_H */
