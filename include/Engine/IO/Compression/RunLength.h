#ifndef ENGINE_IO_COMPRESSION_RUNLENGTH_H
#define ENGINE_IO_COMPRESSION_RUNLENGTH_H

#include <Engine/Includes/Standard.h>

namespace RunLength {
//public:
	bool Decompress(uint8_t* in, size_t in_sz, uint8_t* out, size_t out_sz);
};

#endif /* ENGINE_IO_COMPRESSION_RUNLENGTH_H */
