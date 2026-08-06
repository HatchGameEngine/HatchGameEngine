#ifndef ENGINE_IO_COMPRESSION_HUFFMAN_H
#define ENGINE_IO_COMPRESSION_HUFFMAN_H

#include <Engine/Includes/Standard.h>

class Huffman {
public:
	static bool Decompress(uint8_t* in, size_t in_sz, uint8_t* out, size_t out_sz);
};

#endif /* ENGINE_IO_COMPRESSION_HUFFMAN_H */
