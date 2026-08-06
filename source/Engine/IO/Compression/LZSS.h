#ifndef ENGINE_IO_COMPRESSION_LZSS_H
#define ENGINE_IO_COMPRESSION_LZSS_H

#include <Engine/Includes/Standard.h>

class LZSS {
public:
	static bool Decompress(uint8_t* in, size_t in_sz, uint8_t* out, size_t out_sz);
};

#endif /* ENGINE_IO_COMPRESSION_LZSS_H */
