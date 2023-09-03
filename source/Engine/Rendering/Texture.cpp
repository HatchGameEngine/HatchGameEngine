#if INTERFACE
#include <Engine/Includes/Standard.h>

need_t Texture;

class Texture {
public:
    Uint32   Format;
    Uint32   Access;
    Uint32   Width;
    Uint32   Height;

    void*    Pixels;
    int      Pitch;

    Uint32   ID;
    void*    DriverData;

    Texture* Prev;
    Texture* Next;

    bool     Paletted;
    Uint32*  PaletteColors;
    unsigned NumPaletteColors;
};
#endif

#include <Engine/Rendering/Texture.h>

#include <Engine/Diagnostics/Memory.h>

PUBLIC STATIC Texture* Texture::New(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
    Texture* texture = (Texture*)Memory::TrackedCalloc("Texture::Texture", 1, sizeof(Texture));
    texture->Format = format;
    texture->Access = access;
    texture->Width = width;
    texture->Height = height;
    texture->Pixels = Memory::TrackedCalloc("Texture::Pixels", 1, sizeof(Uint32) * texture->Width * texture->Height);
    return texture;
}

PUBLIC void            Texture::SetPalette(Uint32* palette, unsigned numPaletteColors) {
    if (palette && numPaletteColors) {
        Paletted = true;
        PaletteColors = palette;
        NumPaletteColors = numPaletteColors;
    }
    else {
        Paletted = false;
        PaletteColors = nullptr;
        NumPaletteColors = 0;
    }
}

PUBLIC void            Texture::Dispose() {
    if (PaletteColors)
        Memory::Free(PaletteColors);
    if (Pixels)
        Memory::Free(Pixels);

    PaletteColors = nullptr;
    Pixels = nullptr;
}
