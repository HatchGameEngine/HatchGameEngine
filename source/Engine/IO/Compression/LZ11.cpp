#include <Engine/IO/Compression/LZ11.h>

bool LZ11::Decompress(uint8_t* in, size_t in_sz, uint8_t* out, size_t out_sz) {
	int i;
	uint8_t flags;
	uint8_t* in_head = (uint8_t*)in;
	uint8_t* out_head = (uint8_t*)out;
	size_t size = out_sz;

	int bits = sizeof(flags) << 3;
	// int mask = (1 << (bits - 1));

	while (size > 0) {
		// Read in the flags data
		// From bit 7 to bit 0, following blocks:
		//     0: raw byte
		//     1: compressed block
		flags = *in_head++;
		if (in_head >= in_sz + (uint8_t*)in) {
			return false;
		}

		// the first flag cannot start with 1, as that needs
		// data to pull from

		for (i = 0; i < bits && size > 0; i++, flags <<= 1) {
			// Compressed block
			if (flags & 0x80) {
				uint8_t displen[4];

				displen[0] = *in_head++;
				if (in_head >= in_sz + (uint8_t*)in) {
					return false;
				}

				size_t len; // length
				size_t disp; // displacement
				size_t pos = 0; // displen position

				switch (displen[pos] >> 4) {
				case 0: // extended block
					displen[1] = *in_head++;
					if (in_head >= in_sz + (uint8_t*)in) {
						return false;
					}
					displen[2] = *in_head++;
					if (in_head >= in_sz + (uint8_t*)in) {
						return false;
					}

					len = displen[pos++] << 4;
					len |= displen[pos] >> 4;
					len += 0x11;
					break;

				case 1: // extra extended block
					displen[1] = *in_head++;
					if (in_head >= in_sz + (uint8_t*)in) {
						return false;
					}
					displen[2] = *in_head++;
					if (in_head >= in_sz + (uint8_t*)in) {
						return false;
					}
					displen[3] = *in_head++;
					if (in_head >= in_sz + (uint8_t*)in) {
						return false;
					}

					len = (displen[pos++] & 0x0F) << 12;
					len |= (displen[pos++]) << 4;
					len |= displen[pos] >> 4;
					len += 0x111;
					break;

				default: // normal block
					displen[1] = *in_head++;
					if (in_head >= in_sz + (uint8_t*)in) {
						return false;
					}

					len = (displen[pos] >> 4) + 1;
					break;
				}

				disp = (displen[pos++] & 0x0F) << 8;
				disp |= displen[pos];

				if (len > size) {
					len = size;
				}

				size -= len;

				uint8_t* sup = out_head - (disp + 1);
				while (len-- > 0) {
					*out_head++ = *sup++;
				}
			}
			// Uncompressed block
			else {
				// copy a raw byte from the input to
				// the output
				*out_head++ = *in_head++;
				if (in_head >= in_sz + (uint8_t*)in) {
					return false;
				}
				--size;
			}
		}
	}

	//*/
	return true;
}
