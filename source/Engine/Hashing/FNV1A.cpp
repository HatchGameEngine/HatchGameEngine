#include <Engine/Hashing/FNV1A.h>

Uint32 FNV1A::EncryptString(char* message) {
	Uint32 hash = 0x811C9DC5U;

	while (*message) {
		hash ^= *message;
		hash *= 0x1000193U;
		message++;
	}
	return hash;
}
Uint32 FNV1A::EncryptString(const char* message) {
	return FNV1A::EncryptString((char*)message);
}

Uint32 FNV1A::EncryptData(const void* data, size_t size) {
	return FNV1A::EncryptData(data, size, 0x811C9DC5U);
}
Uint32 FNV1A::EncryptData(const void* data, size_t size, Uint32 hash) {
	size_t i = size;
	char* message = (char*)data;
	while (i) {
		hash ^= *message;
		hash *= 0x1000193U;
		message++;
		i--;
	}
	return hash;
}
