#ifndef ENGINE_IO_COMPRESSION_LZ11_H
#define ENGINE_IO_COMPRESSION_LZ11_H

#include <Engine/Includes/Standard.h>

namespace LZ11 {
//public:
	bool Decompress(uint8_t* in, size_t in_sz, uint8_t* out, size_t out_sz);
};

#endif /* ENGINE_IO_COMPRESSION_LZ11_H */
