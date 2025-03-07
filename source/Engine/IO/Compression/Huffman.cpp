#include <Engine/IO/Compression/Huffman.h>

bool Huffman::Decompress(uint8_t* in, size_t in_sz, uint8_t* out, size_t out_sz) {
#define datat_t uint8_t
	datat_t* in_head = (datat_t*)in;
	datat_t* out_head = (datat_t*)out;
	size_t size = out_sz;

	int bits = 8;
	if (bits < 1 || bits > 8) {
		return false;
	}

	uint8_t* tree = (uint8_t*)malloc(512);
	if (!tree) {
		return false;
	}

	// get tree size
	tree[0] = *in_head++;

	// read tree
	int len_to_read = (tree[0] + 1) * 2 - 1;
	memcpy(&tree[1], in_head, len_to_read);
	in_head += len_to_read;

	uint32_t word = 0; // 32-bits of input bitstream
	uint32_t mask = 0; // which bit we are reading
	uint8_t dataMask = (1 << bits) - 1; // mask to apply to data
	size_t node; // node in the huffman tree
	size_t child; // child of a node
	uint32_t offset; // offset from node to child

	// point to the root of the huffman tree
	node = 1;

	size = 512;

	while (size > 0) {
		// we exhausted 32 bits
		if (mask == 0) {
			// reset the mask
			mask = 0x80000000;

			// read the next 32 bits
			uint8_t wordbuf[4];
			// if (!buffer_get(buffer, &wordbuf[0],
			// callback, userdata) ||
			//     !buffer_get(buffer, &wordbuf[1],
			//     callback, userdata) ||
			//     !buffer_get(buffer, &wordbuf[2],
			//     callback, userdata) ||
			//     !buffer_get(buffer, &wordbuf[3],
			//     callback, userdata)) { free(tree);
			//     return false;
			// }
			wordbuf[0] = *in_head++;
			wordbuf[1] = *in_head++;
			wordbuf[2] = *in_head++;
			wordbuf[3] = *in_head++;

			word = (wordbuf[0] << 0) | (wordbuf[1] << 8) | (wordbuf[2] << 16) |
				(wordbuf[3] << 24);
		}

		// read the current node's offset value
		offset = tree[node] & 0x1F;

		child = (node & ~1) + offset * 2 + 2;

		// we read a 1
		if (!(word & mask)) {
			// point to the "right" child
			child++;

			// "right" child is a data node
			if (tree[node] & 0x40) {
				// copy the child node into the output
				// buffer and apply mask
				// *iov_addr(&out) = tree[child] &
				// dataMask; iov_increment(&out);
				*out_head++ = tree[child] & dataMask;
				--size;

				// start over at the root node
				node = 1;
			}
			else { // traverse to the "right" child
				node = child;
			}
		}
		// we read a 0
		else {
			// pointed to the "left" child

			// "left" child is a data node
			if (tree[node] & 0x80) {
				// copy the child node into the output
				// buffer and apply mask
				// *iov_addr(&out) = tree[child] &
				// dataMask; iov_increment(&out);
				*out_head++ = tree[child] & dataMask;
				--size;

				// start over at the root node
				node = 1;
			}
			else { // traverse to the "left" child
				node = child;
			}
		}

		// shift to read next bit (read bit 31 to bit 0)
		mask >>= 1;
	}

	free(tree);
	return true;
}
