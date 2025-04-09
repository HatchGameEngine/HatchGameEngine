#ifndef ENGINE_HASHING_MD5_H
#define ENGINE_HASHING_MD5_H

#include <Engine/Includes/Standard.h>

namespace MD5 {
//private:
	void*
	Body(Uint32* pa, Uint32* pb, Uint32* pc, Uint32* pd, void* data, unsigned long size);

//public:
	Uint8* EncryptString(Uint8* dest, char* message);
	Uint8* EncryptString(Uint8* dest, const char* message);
	Uint8* EncryptData(Uint8* dest, void* data, size_t size);
};

#endif /* ENGINE_HASHING_MD5_H */
