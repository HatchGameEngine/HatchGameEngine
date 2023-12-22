#if INTERFACE
#include <Engine/Includes/Standard.h>

class ImageFormat {
public:
    Uint32* Colors = NULL;
    Uint32* Data = NULL;
    Uint32  Width = 0;
    Uint32  Height = 0;
    bool    Paletted = false;
    Uint16  NumPaletteColors = 0;
};
#endif

#include <Engine/ResourceTypes/ImageFormats/ImageFormat.h>
#include <Engine/Diagnostics/Memory.h>

PUBLIC       Uint32* ImageFormat::GetPalette() {
    if (!Colors)
        return nullptr;

    Uint32* colors = (Uint32*)Memory::TrackedMalloc("ImageFormat::Colors", NumPaletteColors * sizeof(Uint32));
    memcpy(colors, Colors, NumPaletteColors * sizeof(Uint32));
    return colors;
}

PUBLIC VIRTUAL bool ImageFormat::Save(const char* filename) {
    return false;
}
PUBLIC VIRTUAL      ImageFormat::~ImageFormat() {
    Memory::Free(Colors);
    Colors = nullptr;
}
