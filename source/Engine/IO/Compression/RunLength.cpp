#include <Engine/IO/Compression/RunLength.h>

bool RunLength::Decompress(uint8_t* in, size_t in_sz, uint8_t* out, size_t out_sz) {
	uint8_t byte;
	size_t len;
	uint8_t* in_head = in;
	uint8_t* out_head = out;
	size_t size = out_sz;

	while (size > 0) {
		// Read in the flag
		byte = *in_head++;
		if (in_head >= in + in_sz) {
			return false;
		}

		// Compressed block
		if (byte & 0x80) {
			// read the length of the run
			len = (byte & 0x7F) + 3;

			if (len > size) {
				len = size;
			}

			size -= len;

			// read in the byte used for the run
			byte = *in_head++;
			if (in_head >= in + in_sz) {
				return false;
			}

			// For len, copy byte into output
			while (len-- > 0) {
				*out_head++ = byte;
			}
		}
		// Uncompressed block
		else {
			// read the length of uncompressed bytes
			len = (byte & 0x7F) + 1;

			if (len > size) {
				len = size;
			}

			size -= len;

			// For len, copy from input to output
			while (len-- > 0) {
				*out_head++ = *in_head++;
			}
		}
	}

	return true;
}
