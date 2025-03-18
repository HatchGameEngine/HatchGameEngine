#include <Engine/ResourceTypes/ImageFormats/GIF.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>

#include <Engine/Graphics.h>

#include <Libraries/stb_image.h>

struct Node {
	Uint16 Key;
	struct Node* Children[];
};
struct Entry {
	Uint8 Used;
	Uint16 Length;
	Uint16 Prefix;
	Uint8 Suffix;
};

inline Uint32
GIF::ReadCode(Stream* stream, int codeSize, int* blockLength, int* bitCache, int* bitCacheLength) {
	if (*blockLength == 0) {
		*blockLength = stream->ReadByte();
	}

	while (*bitCacheLength <= codeSize && *blockLength > 0) {
		Uint32 byte = stream->ReadByte();
		(*blockLength)--;
		*bitCache |= byte << *bitCacheLength;
		*bitCacheLength += 8;

		if (*blockLength == 0) {
			*blockLength = stream->ReadByte();
		}
	}
	Uint32 result = *bitCache & ((1 << codeSize) - 1);
	*bitCache >>= codeSize;
	*bitCacheLength -= codeSize;

	return result;
}
inline void GIF::WriteCode(Stream* stream,
	int* offset,
	int* partial,
	Uint8* buffer,
	uint16_t key,
	int key_size) {
	int byte_offset, bit_offset, bits_to_write;
	byte_offset = *offset >> 3;
	bit_offset = *offset & 0x7;
	*partial |= ((uint32_t)key) << bit_offset;
	bits_to_write = bit_offset + key_size;
	while (bits_to_write >= 8) {
		buffer[byte_offset++] = *partial & 0xFF;
		if (byte_offset == 0xFF) {
			stream->WriteByte(0xFF);
			stream->WriteBytes(buffer, 0xFF);
			byte_offset = 0;
		}
		*partial >>= 8;
		bits_to_write -= 8;
	}
	*offset = (*offset + key_size) % (0xFF * 8);
}
inline void GIF::WriteFrame(Stream* stream, Uint32* data) {
	int depth = 8;
	// Put Image
	Node* node;
	Node* root;
	Node* child;
	int nkeys, key_size, i, j;
	int degree = 1 << depth;

	Uint8 buffer[0x100];
	int offset = 0, partial = 0;

	// stream->WriteByte(0x21);
	// stream->WriteByte(0xF9);
	// stream->WriteByte(0x04);
	// stream->WriteByte(0x01); // Transparent
	// stream->WriteUInt16(100 / 50); // 50 fps
	// stream->WriteByte((Uint8)this->TransparentColorIndex);
	// stream->WriteByte(0x00);

	stream->WriteByte(0x2C);
	stream->WriteUInt16(0); // X
	stream->WriteUInt16(0); // Y
	stream->WriteUInt16((Uint16)this->Width); // Width
	stream->WriteUInt16((Uint16)this->Height); // Height

	stream->WriteByte(0x00); // Packed field
	stream->WriteByte((Uint8)depth); // Key size

	root = node = (Node*)NewTree(degree, &nkeys);
	key_size = depth + 1;
	WriteCode(stream, &offset, &partial, buffer, degree, key_size); /* clear code */
	for (i = 0; i < (int)this->Height; i++) {
		for (j = 0; j < (int)this->Width; j++) {
			Uint8 pixel = (Uint8)(data[i * this->Width + j] & (degree - 1));
			child = node->Children[pixel];
			if (child) {
				node = child;
			}
			else {
				WriteCode(stream, &offset, &partial, buffer, node->Key, key_size);
				if (nkeys < 0x1000) {
					if (nkeys == (1 << key_size)) {
						key_size++;
					}
					node->Children[pixel] = (Node*)NewNode(nkeys++, degree);
				}
				else {
					WriteCode(stream,
						&offset,
						&partial,
						buffer,
						degree,
						key_size); /* clear
					                      code */
					FreeTree(root, degree);
					root = node = (Node*)NewTree(degree, &nkeys);
					key_size = depth + 1;
				}
				node = root->Children[pixel];
			}
		}
	}
	WriteCode(stream, &offset, &partial, buffer, node->Key, key_size);
	WriteCode(stream, &offset, &partial, buffer, degree + 1, key_size); /* stop code */
	// end_key(gif);
	int byte_offset;
	byte_offset = offset >> 3;
	if (offset & 7) {
		buffer[byte_offset++] = partial & 0xFF;
	}
	stream->WriteByte((Uint8)byte_offset);
	stream->WriteBytes(buffer, byte_offset);
	stream->WriteByte(0);
	offset = partial = 0;
	//
	FreeTree(root, degree);
}

void* GIF::NewNode(Uint16 key, int degree) {
	Node* node = (Node*)Memory::Calloc(1, sizeof(*node) + degree * sizeof(Node*));
	if (node) {
		node->Key = key;
	}
	return node;
}
void* GIF::NewTree(int degree, int* nkeys) {
	Node* root = (Node*)GIF::NewNode(0, degree);
	for (*nkeys = 0; *nkeys < degree; (*nkeys)++) {
		root->Children[*nkeys] = (Node*)GIF::NewNode(*nkeys, degree);
	}
	*nkeys += 2;
	return root;
}
void GIF::FreeTree(void* root, int degree) {
	if (!root) {
		return;
	}
	for (int i = 0; i < degree; i++) {
		FreeTree(((Node*)root)->Children[i], degree);
	}
	Memory::Free(root);
}

#define MARK_PERF(line) \
	{ \
		float delta = Clock::End(); \
		Log::Print(Log::LOG_VERBOSE, \
			"- Mark at line %d took %.3f ms to reach from last mark.", \
			line, \
			delta); \
		Clock::Start(); \
	}
#define MARK_PERF_LABEL(label) \
	{ \
		float delta = Clock::End(); \
		Log::Print(Log::LOG_VERBOSE, "- Mark '%s' took %.3f ms to reach.", label, delta); \
		Clock::Start(); \
	}

GIF* GIF::Load(const char* filename) {
	bool loadPalette = Graphics::UsePalettes;
	Entry* codeTable = (Entry*)Memory::Malloc(0x1000 * sizeof(Entry));

	GIF* gif = new GIF;
	Stream* stream = NULL;

	size_t fileSize;
	void* fileBuffer = NULL;

	Clock::Start();

	stream = ResourceStream::New(filename);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Could not open file '%s'!", filename);
		goto GIF_Load_FAIL;
	}

#ifdef DO_GIF_PERF
	MARK_PERF_LABEL("stream open");
#endif

	fileSize = stream->Length();
	fileBuffer = Memory::Malloc(fileSize);
	stream->ReadBytes(fileBuffer, fileSize);
	stream->Close();

#ifdef DO_GIF_PERF
	MARK_PERF_LABEL("read to buffer");
#endif

	stream = MemoryStream::New(fileBuffer, fileSize);
	if (!stream) {
		goto GIF_Load_FAIL;
	}

#ifdef DO_GIF_PERF
	MARK_PERF_LABEL("memorystream transfer");
#endif

	Uint8 magicGIF[4];
	stream->ReadBytes(magicGIF, 3);
	if (memcmp(magicGIF, "GIF", 3) != 0) {
		magicGIF[3] = 0;
		Log::Print(Log::LOG_ERROR,
			"Invalid GIF file! Found \"%s\", expected \"GIF\"! (%s)",
			magicGIF,
			filename);
		goto GIF_Load_FAIL;
	}

	Uint8 magic89a[4];
	stream->ReadBytes(magic89a, 3);
	if (memcmp(magic89a, "89a", 3) != 0 && memcmp(magic89a, "87a", 3) != 0) {
		magic89a[3] = 0;
		Log::Print(Log::LOG_ERROR,
			"Invalid GIF version! Found \"%s\", expected \"89a\"! (%s)",
			magic89a,
			filename);
		goto GIF_Load_FAIL;
	}

	Uint16 width, height, paletteTableSize;
	int bitsWidth, eighthHeight, quarterHeight, halfHeight;
	Uint8 logicalScreenDesc, colorBitDepth, transparentColorIndex;

	width = stream->ReadUInt16();
	height = stream->ReadUInt16();

	logicalScreenDesc = stream->ReadByte();

	transparentColorIndex = stream->ReadByte();
	stream->Skip(1); // pixelAspectRatio = stream->ReadByte();

	if ((logicalScreenDesc & 0x80) == 0) {
		Log::Print(Log::LOG_ERROR, "GIF missing palette table! (%s)", filename);
		goto GIF_Load_FAIL;
	}

	gif->Width = width;
	gif->Height = height;

	colorBitDepth =
		((logicalScreenDesc & 0x70) >> 4) + 1; // normally 7, sometimes it is 4 (wrong)
	// sortFlag = (logicalScreenDesc & 0x8) != 0; // This is
	// unneeded.
	paletteTableSize = 2 << (logicalScreenDesc & 0x7);

#ifdef DO_GIF_PERF
	MARK_PERF_LABEL("gif header read");
#endif

	// Prepare image data
	gif->Data = (Uint32*)Memory::TrackedMalloc("GIF::Data", width * height * sizeof(Uint32));
	// Load Palette Table
	gif->Colors = (Uint32*)Memory::TrackedMalloc("GIF::Colors", 0x100 * sizeof(Uint32));
	if (Graphics::PreferredPixelFormat == SDL_PIXELFORMAT_ABGR8888) {
		for (int p = 0; p < paletteTableSize; p++) {
			gif->Colors[p] = 0xFF000000U;
			// Load 'red'
			gif->Colors[p] |= stream->ReadByte();
			// Load 'green'
			gif->Colors[p] |= stream->ReadByte() << 8;
			// Load 'blue'
			gif->Colors[p] |= stream->ReadByte() << 16;
		}
	}
	else {
		for (int p = 0; p < paletteTableSize; p++) {
			gif->Colors[p] = 0xFF000000U;
			// Load 'red'
			gif->Colors[p] |= stream->ReadByte() << 16;
			// Load 'green'
			gif->Colors[p] |= stream->ReadByte() << 8;
			// Load 'blue'
			gif->Colors[p] |= stream->ReadByte();
		}
	}

#ifdef DO_GIF_PERF
	MARK_PERF_LABEL("allocate image & palette buffer");
#endif

	gif->Paletted = loadPalette;
	gif->NumPaletteColors = 256;

	memset(gif->Colors + paletteTableSize, 0, (0x100 - paletteTableSize) * sizeof(Uint32));

	width--;
	height--;

	eighthHeight = gif->Height >> 3;
	quarterHeight = gif->Height >> 2;
	halfHeight = gif->Height >> 1;

#ifdef DO_GIF_PERF
	MARK_PERF_LABEL("clear unused palette memory");
#endif

	// Get frame
	Uint8 type, subtype, temp;
	type = stream->ReadByte();
	while (type) {
		bool tableFull, interlaced;
		int codeSize, initCodeSize;
		int clearCode, eoiCode, emptyCode;
		int blockLength, bitCache, bitCacheLength;
		int mark, str_len = 0, frm_off = 0;
		int currentCode;

		switch (type) {
		// Extension
		case 0x21:
			subtype = stream->ReadByte();
			switch (subtype) {
			// Graphics Control Extension
			case 0xF9:
				// stream->Skip(0x06);
				// temp = stream->ReadByte();  // Block
				// Size [byte] (always 0x04) temp =
				// stream->ReadByte();  // Packed Field
				// [byte] // temp16 =
				// stream->ReadUInt16(); // Delay Time
				// [short] //
				stream->Skip(0x04);
				transparentColorIndex = stream->ReadByte(); // Transparent
					// Color
					// Index?
					// [byte]
					// //
				stream->Skip(0x01);
				// temp = stream->ReadByte();  // Block
				// Terminator [byte] //
				break;
			// Plain Text Extension
			case 0x01:
			// Comment Extension
			case 0xFE:
			// Application Extension
			case 0xFF:
				temp = stream->ReadByte(); // Block
					// Size
				// Continue until we run out of blocks
				while (temp) {
					// Read block
					stream->Skip(temp); // stream->ReadBytes(buffer,
						// temp);
					temp = stream->ReadByte(); // next block Size
				}
				break;
			default:
				Log::Print(Log::LOG_ERROR,
					"Unsupported GIF control extension '%02X'!",
					subtype);
				goto GIF_Load_FAIL;
			}
			break;
		// Image descriptor
		case 0x2C:
			// temp16 = stream->ReadUInt16(); //
			// Destination X temp16 = stream->ReadUInt16();
			// // Destination Y temp16 =
			// stream->ReadUInt16(); // Destination Width
			// temp16 = stream->ReadUInt16(); //
			// Destination Height
			stream->Skip(8);
			temp = stream->ReadByte(); // Packed Field
				// [byte]

			// If a local color table exists,
			if (temp & 0x80) {
				int size = 2 << (temp & 0x07);
				// Load all colors
				stream->Skip(3 * size); // stream->ReadBytes(buffer,
					// 3 * size);
			}

			interlaced = (temp & 0x40) == 0x40;
			if (interlaced) {
				if ((width & (width - 1)) != 0) {
					Log::Print(Log::LOG_ERROR,
						"Interlaced GIF width must be power of two!");
					goto GIF_Load_FAIL;
				}
				if ((height & (height - 1)) != 0) {
					Log::Print(Log::LOG_ERROR,
						"Interlaced GIF width must be power of two!");
					goto GIF_Load_FAIL;
				}

				bitsWidth = 0;
				while (width) {
					width >>= 1;
					bitsWidth++;
				}
				width = gif->Width - 1;
			}
			else {
				bitsWidth = 0;
			}

			codeSize = stream->ReadByte();

			clearCode = 1 << codeSize;
			eoiCode = clearCode + 1;
			emptyCode = eoiCode + 1;

			codeSize++;
			initCodeSize = codeSize;

			// Init table
			for (int i = 0; i <= eoiCode; i++) {
				codeTable[i].Length = 1;
				codeTable[i].Prefix = 0xFFF;
				codeTable[i].Suffix = (Uint8)i;
			}

			blockLength = 0;
			bitCache = 0;
			bitCacheLength = 0;
			tableFull = false;

			currentCode = ReadCode(
				stream, codeSize, &blockLength, &bitCache, &bitCacheLength);

			codeSize = initCodeSize;
			emptyCode = eoiCode + 1;
			tableFull = false;

			Entry entry;
			entry.Suffix = 0;

			while (blockLength) {
				mark = 0;

				if (currentCode == clearCode) {
					codeSize = initCodeSize;
					emptyCode = eoiCode + 1;
					tableFull = false;
				}
				else if (!tableFull) {
					codeTable[emptyCode].Length = str_len + 1;
					codeTable[emptyCode].Prefix = currentCode;
					codeTable[emptyCode].Suffix = entry.Suffix;
					emptyCode++;

					// Once we reach highest code,
					// increase code size
					if ((emptyCode & (emptyCode - 1)) == 0) {
						mark = 1;
					}
					else {
						mark = 0;
					}

					if (emptyCode >= 0x1000) {
						mark = 0;
						tableFull = true;
					}
				}

				currentCode = ReadCode(
					stream, codeSize, &blockLength, &bitCache, &bitCacheLength);

				if (currentCode == clearCode) {
					continue;
				}
				if (currentCode == eoiCode) {
					goto GIF_Load_Success;
				}
				if (mark == 1) {
					codeSize++;
				}

				entry = codeTable[currentCode];
				str_len = entry.Length;

				while (true) {
					int p = frm_off + entry.Length - 1;
					if (interlaced) {
						int row = p >> bitsWidth;
						if (row < eighthHeight) {
							p = (p & width) +
								((((row) << 3) + 0) << bitsWidth);
						}
						else if (row < quarterHeight) {
							p = (p & width) +
								((((row - eighthHeight) << 3) + 4)
									<< bitsWidth);
						}
						else if (row < halfHeight) {
							p = (p & width) +
								((((row - quarterHeight) << 2) + 2)
									<< bitsWidth);
						}
						else {
							p = (p & width) +
								((((row - halfHeight) << 1) + 1)
									<< bitsWidth);
						}
					}

					gif->Data[p] = entry.Suffix;
					// For setting straight to
					// color (no palette use)
					if (!loadPalette) {
						if (gif->Data[p] == transparentColorIndex) {
							gif->Data[p] = 0;
						}
						else {
							gif->Data[p] = gif->Colors[gif->Data[p]];
						}
					}

					if (entry.Prefix != 0xFFF) {
						entry = codeTable[entry.Prefix];
					}
					else {
						break;
					}
				}
				frm_off += str_len;
				if (currentCode < emptyCode - 1 && !tableFull) {
					codeTable[emptyCode - 1].Suffix = entry.Suffix;
				}
			}
			break;
		}

		type = stream->ReadByte();

		if (type == 0x3B) {
			break;
		}
	}

	goto GIF_Load_Success;

GIF_Load_FAIL:
	delete gif;
	gif = NULL;

GIF_Load_Success:
#ifdef DO_GIF_PERF
	MARK_PERF_LABEL("decode gif data");
#endif
	Clock::End();
	if (stream) {
		stream->Close();
	}
	Memory::Free(fileBuffer);
	Memory::Free(codeTable);
	return gif;
}

bool GIF::Save(GIF* gif, const char* filename) {
	return gif->Save(filename);
}

bool GIF::Save(const char* filename) {
	Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
	if (!stream) {
		return false;
	}

	stream->WriteByte('G');
	stream->WriteByte('I');
	stream->WriteByte('F');
	stream->WriteByte('8');
	stream->WriteByte('9');
	stream->WriteByte('a');

	stream->WriteUInt16((Uint16)this->Width);
	stream->WriteUInt16((Uint16)this->Height);

	Uint8 logicalScreenDesc = 0x70;
	if (Colors) {
		logicalScreenDesc |= 0x80;
		logicalScreenDesc |= 0x07; // 256 colors
	}

	stream->WriteByte(logicalScreenDesc);
	stream->WriteByte(0x00);
	// stream->WriteByte((Uint8)this->TransparentColorIndex);
	stream->WriteByte(0x00);

	for (int p = 0; p < 256; p++) {
		stream->WriteByte(this->Colors[p] >> 16 & 0xFF);
		stream->WriteByte(this->Colors[p] >> 8 & 0xFF);
		stream->WriteByte(this->Colors[p] & 0xFF);
	}

	if (this->Frames.size()) {
		stream->WriteByte('!');
		stream->WriteByte(0xFF);
		stream->WriteByte(0x0B);

		stream->WriteBytes((Uint8*)"NETSCAPE2.0", 11);

		stream->WriteByte(0x03);
		stream->WriteByte(0x01);

		stream->WriteUInt16(0x00); // Loop forver
		stream->WriteByte(0x00);
	}

	if (this->Frames.size()) {
		for (size_t i = 0; i < this->Frames.size(); i++) {
			this->WriteFrame(stream, this->Frames[i]);
		}
	}
	else {
		this->WriteFrame(stream, this->Data);
	}

	stream->WriteByte(0x3B);

	stream->Close();
	return true;
}

GIF::~GIF() {}
