#ifndef ENGINE_HASHING_COMBINEDHASH_H
#define ENGINE_HASHING_COMBINEDHASH_H

#include <Engine/Includes/Standard.h>

namespace CombinedHash {
//public:
	Uint32 EncryptString(char* data);
	Uint32 EncryptString(const char* message);
	Uint32 EncryptData(const void* message, size_t len);
};

#endif /* ENGINE_HASHING_COMBINEDHASH_H */
