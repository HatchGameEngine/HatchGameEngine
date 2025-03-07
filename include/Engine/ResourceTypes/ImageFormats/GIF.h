#ifndef ENGINE_RESOURCETYPES_IMAGEFORMATS_GIF_H
#define ENGINE_RESOURCETYPES_IMAGEFORMATS_GIF_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/ImageFormats/ImageFormat.h>

class GIF : public ImageFormat {
private:
	static inline Uint32 ReadCode(Stream* stream,
		int codeSize,
		int* blockLength,
		int* bitCache,
		int* bitCacheLength);
	static inline void WriteCode(Stream* stream,
		int* offset,
		int* partial,
		Uint8* buffer,
		uint16_t key,
		int key_size);
	inline void WriteFrame(Stream* stream, Uint32* data);
	static void* NewNode(Uint16 key, int degree);
	static void* NewTree(int degree, int* nkeys);
	static void FreeTree(void* root, int degree);

public:
	vector<Uint32*> Frames;

	static GIF* Load(const char* filename);
	static bool Save(GIF* gif, const char* filename);
	bool Save(const char* filename);
	~GIF();
};

#endif /* ENGINE_RESOURCETYPES_IMAGEFORMATS_GIF_H */
