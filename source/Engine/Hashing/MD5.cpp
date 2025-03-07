#include <Engine/Hashing/MD5.h>

#define F(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z) ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z) (((x) ^ (y)) ^ (z))
#define H2(x, y, z) ((x) ^ ((y) ^ (z)))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))

#define STEP(f, a, b, c, d, x, t, s) \
	(a) += f((b), (c), (d)) + (x) + (t); \
	(a) = (((a) << (s)) | (((a) & 0xffffffff) >> (32 - (s)))); \
	(a) += (b);

#define SET(n) TO_LE32(*(Uint32*)&ptr[(n) * 4])
#define GET(n) SET(n)

#define OUT(dst, src) \
	(dst)[0] = (Uint8)(src); \
	(dst)[1] = (Uint8)((src) >> 8); \
	(dst)[2] = (Uint8)((src) >> 16); \
	(dst)[3] = (Uint8)((src) >> 24);

void* MD5::Body(Uint32* pa, Uint32* pb, Uint32* pc, Uint32* pd, void* data, unsigned long size) {
	Uint32 saved_a, saved_b, saved_c, saved_d;

	Uint8* ptr = (Uint8*)data;

	Uint32 a, b, c, d;

	a = *pa;
	b = *pb;
	c = *pc;
	d = *pd;

	do {
		saved_a = a;
		saved_b = b;
		saved_c = c;
		saved_d = d;

		/* Round 1 */
		STEP(F, a, b, c, d, SET(0), 0xd76aa478, 7)
		STEP(F, d, a, b, c, SET(1), 0xe8c7b756, 12)
		STEP(F, c, d, a, b, SET(2), 0x242070db, 17)
		STEP(F, b, c, d, a, SET(3), 0xc1bdceee, 22)
		STEP(F, a, b, c, d, SET(4), 0xf57c0faf, 7)
		STEP(F, d, a, b, c, SET(5), 0x4787c62a, 12)
		STEP(F, c, d, a, b, SET(6), 0xa8304613, 17)
		STEP(F, b, c, d, a, SET(7), 0xfd469501, 22)
		STEP(F, a, b, c, d, SET(8), 0x698098d8, 7)
		STEP(F, d, a, b, c, SET(9), 0x8b44f7af, 12)
		STEP(F, c, d, a, b, SET(10), 0xffff5bb1, 17)
		STEP(F, b, c, d, a, SET(11), 0x895cd7be, 22)
		STEP(F, a, b, c, d, SET(12), 0x6b901122, 7)
		STEP(F, d, a, b, c, SET(13), 0xfd987193, 12)
		STEP(F, c, d, a, b, SET(14), 0xa679438e, 17)
		STEP(F, b, c, d, a, SET(15), 0x49b40821, 22)

		/* Round 2 */
		STEP(G, a, b, c, d, GET(1), 0xf61e2562, 5)
		STEP(G, d, a, b, c, GET(6), 0xc040b340, 9)
		STEP(G, c, d, a, b, GET(11), 0x265e5a51, 14)
		STEP(G, b, c, d, a, GET(0), 0xe9b6c7aa, 20)
		STEP(G, a, b, c, d, GET(5), 0xd62f105d, 5)
		STEP(G, d, a, b, c, GET(10), 0x02441453, 9)
		STEP(G, c, d, a, b, GET(15), 0xd8a1e681, 14)
		STEP(G, b, c, d, a, GET(4), 0xe7d3fbc8, 20)
		STEP(G, a, b, c, d, GET(9), 0x21e1cde6, 5)
		STEP(G, d, a, b, c, GET(14), 0xc33707d6, 9)
		STEP(G, c, d, a, b, GET(3), 0xf4d50d87, 14)
		STEP(G, b, c, d, a, GET(8), 0x455a14ed, 20)
		STEP(G, a, b, c, d, GET(13), 0xa9e3e905, 5)
		STEP(G, d, a, b, c, GET(2), 0xfcefa3f8, 9)
		STEP(G, c, d, a, b, GET(7), 0x676f02d9, 14)
		STEP(G, b, c, d, a, GET(12), 0x8d2a4c8a, 20)

		/* Round 3 */
		STEP(H, a, b, c, d, GET(5), 0xfffa3942, 4)
		STEP(H2, d, a, b, c, GET(8), 0x8771f681, 11)
		STEP(H, c, d, a, b, GET(11), 0x6d9d6122, 16)
		STEP(H2, b, c, d, a, GET(14), 0xfde5380c, 23)
		STEP(H, a, b, c, d, GET(1), 0xa4beea44, 4)
		STEP(H2, d, a, b, c, GET(4), 0x4bdecfa9, 11)
		STEP(H, c, d, a, b, GET(7), 0xf6bb4b60, 16)
		STEP(H2, b, c, d, a, GET(10), 0xbebfbc70, 23)
		STEP(H, a, b, c, d, GET(13), 0x289b7ec6, 4)
		STEP(H2, d, a, b, c, GET(0), 0xeaa127fa, 11)
		STEP(H, c, d, a, b, GET(3), 0xd4ef3085, 16)
		STEP(H2, b, c, d, a, GET(6), 0x04881d05, 23)
		STEP(H, a, b, c, d, GET(9), 0xd9d4d039, 4)
		STEP(H2, d, a, b, c, GET(12), 0xe6db99e5, 11)
		STEP(H, c, d, a, b, GET(15), 0x1fa27cf8, 16)
		STEP(H2, b, c, d, a, GET(2), 0xc4ac5665, 23)

		/* Round 4 */
		STEP(I, a, b, c, d, GET(0), 0xf4292244, 6)
		STEP(I, d, a, b, c, GET(7), 0x432aff97, 10)
		STEP(I, c, d, a, b, GET(14), 0xab9423a7, 15)
		STEP(I, b, c, d, a, GET(5), 0xfc93a039, 21)
		STEP(I, a, b, c, d, GET(12), 0x655b59c3, 6)
		STEP(I, d, a, b, c, GET(3), 0x8f0ccc92, 10)
		STEP(I, c, d, a, b, GET(10), 0xffeff47d, 15)
		STEP(I, b, c, d, a, GET(1), 0x85845dd1, 21)
		STEP(I, a, b, c, d, GET(8), 0x6fa87e4f, 6)
		STEP(I, d, a, b, c, GET(15), 0xfe2ce6e0, 10)
		STEP(I, c, d, a, b, GET(6), 0xa3014314, 15)
		STEP(I, b, c, d, a, GET(13), 0x4e0811a1, 21)
		STEP(I, a, b, c, d, GET(4), 0xf7537e82, 6)
		STEP(I, d, a, b, c, GET(11), 0xbd3af235, 10)
		STEP(I, c, d, a, b, GET(2), 0x2ad7d2bb, 15)
		STEP(I, b, c, d, a, GET(9), 0xeb86d391, 21)

		a += saved_a;
		b += saved_b;
		c += saved_c;
		d += saved_d;

		ptr += 64;
	} while (size -= 64);

	*pa = a;
	*pb = b;
	*pc = c;
	*pd = d;

	return ptr;
}

Uint8* MD5::EncryptString(Uint8* dest, char* message) {
	return MD5::EncryptData(dest, message, strlen(message));
}
Uint8* MD5::EncryptString(Uint8* dest, const char* message) {
	return MD5::EncryptString(dest, (char*)message);
}

Uint8* MD5::EncryptData(Uint8* dest, void* data, size_t size) {
	// Init
	Uint8 buffer[64];
	size_t lo = 0, hi = 0;
	Uint32 a = 0x67452301, b = 0xefcdab89, c = 0x98badcfe, d = 0x10325476;

	// Update
	size_t used, available;
	size_t saved_lo = lo;
	if ((lo = (saved_lo + size) & 0x1fffffff) < saved_lo) {
		hi++;
	}
	hi += size >> 29;

	used = saved_lo & 0x3f;

	if (used) {
		available = 64 - used;

		if (size < available) {
			memcpy(&buffer[used], data, size);
			return dest;
		}

		memcpy(&buffer[used], data, available);
		data = (Uint8*)data + available;
		size -= available;
		MD5::Body(&a, &b, &c, &d, buffer, 64);
	}

	if (size >= 64) {
		data = MD5::Body(&a, &b, &c, &d, data, size & ~(unsigned long)0x3f);
		size &= 0x3f;
	}

	memcpy(buffer, data, size);

	// Final
	used = lo & 0x3f;

	buffer[used++] = 0x80;

	available = 64 - used;

	if (available < 8) {
		memset(&buffer[used], 0, available);
		MD5::Body(&a, &b, &c, &d, buffer, 64);
		used = 0;
		available = 64;
	}

	memset(&buffer[used], 0, available - 8);

	lo <<= 3;
	OUT(&buffer[56], lo)
	OUT(&buffer[60], hi)

	MD5::Body(&a, &b, &c, &d, buffer, 64);

	OUT(&dest[0], a)
	OUT(&dest[4], b)
	OUT(&dest[8], c)
	OUT(&dest[12], d)
	return dest;
}
