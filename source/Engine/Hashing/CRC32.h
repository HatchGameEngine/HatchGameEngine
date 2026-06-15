#ifndef ENGINE_HASHING_CRC32_H
#define ENGINE_HASHING_CRC32_H

#include <Engine/Includes/Standard.h>

class CRC32 {
public:
	static Uint32 EncryptString(char* data);
	static Uint32 EncryptString(const char* message);
	static Uint32 EncryptData(const void* data, size_t size);
	static Uint32 EncryptData(const void* data, size_t size, Uint32 crc);
};

#endif /* ENGINE_HASHING_CRC32_H */
