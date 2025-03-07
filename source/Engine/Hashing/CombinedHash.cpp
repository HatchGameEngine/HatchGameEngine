#include <Engine/Hashing/CombinedHash.h>

#include <Engine/Hashing/CRC32.h>
#include <Engine/Hashing/MD5.h>

Uint32 CombinedHash::EncryptString(char* data) {
	Uint8 objMD5[16];
	return CRC32::EncryptData(MD5::EncryptString(objMD5, data), 16);
}
Uint32 CombinedHash::EncryptString(const char* message) {
	return CombinedHash::EncryptString((char*)message);
}

Uint32 CombinedHash::EncryptData(const void* message, size_t len) {
	Uint8 objMD5[16];
	return CRC32::EncryptData(MD5::EncryptData(objMD5, (void*)message, len), 16);
}
