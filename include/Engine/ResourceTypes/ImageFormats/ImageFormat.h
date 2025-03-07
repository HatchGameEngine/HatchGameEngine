#ifndef ENGINE_RESOURCETYPES_IMAGEFORMATS_IMAGEFORMAT_H
#define ENGINE_RESOURCETYPES_IMAGEFORMATS_IMAGEFORMAT_H

#include <Engine/Includes/Standard.h>

class ImageFormat {
public:
	Uint32* Colors = NULL;
	Uint32* Data = NULL;
	Uint32 Width = 0;
	Uint32 Height = 0;
	bool Paletted = false;
	Uint16 NumPaletteColors = 0;

	Uint32* GetPalette();
	virtual bool Save(const char* filename);
	virtual ~ImageFormat();
};

#endif /* ENGINE_RESOURCETYPES_IMAGEFORMATS_IMAGEFORMAT_H */
