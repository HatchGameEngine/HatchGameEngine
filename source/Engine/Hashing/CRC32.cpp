#include <Engine/Hashing/CRC32.h>

Uint32 CRC32::EncryptString(char* data) {
	return CRC32::EncryptData(data, strlen(data));
}
Uint32 CRC32::EncryptString(const char* message) {
	return CRC32::EncryptString((char*)message);
}

Uint32 CRC32::EncryptData(const void* data, size_t size) {
	return CRC32::EncryptData(data, size, 0xFFFFFFFFU);
}
Uint32 CRC32::EncryptData(const void* data, size_t size, Uint32 crc) {
	int j;
	Uint32 byte, mask;
	Uint8* message = (Uint8*)data;

	while (size) {
		byte = *message;
		crc = crc ^ byte;
		for (j = 7; j >= 0; j--) {
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
		message++;
		size--;
	}
	return ~crc;
}
