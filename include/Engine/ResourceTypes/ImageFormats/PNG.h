#ifndef ENGINE_RESOURCETYPES_IMAGEFORMATS_PNG_H
#define ENGINE_RESOURCETYPES_IMAGEFORMATS_PNG_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/ImageFormats/ImageFormat.h>

#define MAX_PNG_TEXT_KEYWORD_LENGTH 80

struct PNGMetadata {
	char Keyword[MAX_PNG_TEXT_KEYWORD_LENGTH];
	std::string Text;
};

class PNG : public ImageFormat {
public:
	static PNG* Load(Stream* stream);
	void ReadPixelDataARGB(Uint32* pixelData, int num_channels);
	void ReadPixelBitstream(Uint8* pixelData, size_t bit_depth);
	static bool Save(PNG* png, const char* filename);
	static bool Save(Texture* texture, const char* filename);
	static bool
	Save(Texture* texture, const char* filename, std::vector<PNGMetadata>* metadata);
	bool Save(const char* filename);
	~PNG();
};

#endif /* ENGINE_RESOURCETYPES_IMAGEFORMATS_PNG_H */
