#ifndef ENGINE_RESOURCETYPES_IMAGEFORMATS_PNG_H
#define ENGINE_RESOURCETYPES_IMAGEFORMATS_PNG_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/ImageFormats/ImageFormat.h>

class PNG : public ImageFormat {
public:
	static PNG* Load(Stream* stream);
	void ReadPixelBitstream(Uint8* pixelData, size_t bit_depth);
	static bool Save(PNG* png, const char* filename);
	bool Save(const char* filename);
	~PNG();
};

#endif /* ENGINE_RESOURCETYPES_IMAGEFORMATS_PNG_H */
