#include <Engine/Hashing/FNV1A.h>
#include <Engine/Hashing/Murmur.h>

Uint32 Murmur::EncryptString(char* message) {
	return Murmur::EncryptData(message, strlen(message), 0xDEADBEEF);
}
Uint32 Murmur::EncryptString(const char* message) {
	return Murmur::EncryptString((char*)message);
}

Uint32 Murmur::EncryptData(const void* data, size_t size) {
	return Murmur::EncryptData(data, size, 0xDEADBEEF);
}
Uint32 Murmur::EncryptData(const void* key, size_t size, Uint32 hash) {
	const unsigned int m = 0x5bd1e995;
	const int r = 24;
	unsigned int h = hash ^ (Uint32)size;

	const unsigned char* data = (const unsigned char*)key;

	while (size >= 4) {
		unsigned int k = FROM_LE32(*(unsigned int*)data);

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		size -= 4;
	}

	// Handle the last few bytes of the input array
	switch (size) {
	case 3:
		h ^= data[2] << 16;
	case 2:
		h ^= data[1] << 8;
	case 1:
		h ^= data[0];
		h *= m;
	}

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	return h;
}
