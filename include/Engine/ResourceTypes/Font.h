#ifndef ENGINE_RESOURCETYPES_FONT_H
#define ENGINE_RESOURCETYPES_FONT_H

#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/ISprite.h>

struct FontGlyph {
    unsigned Width;
    unsigned Height;
    unsigned SourceX;
    unsigned SourceY;
    float OffsetX;
    float OffsetY;
    float Advance;
};

class Font {
private:
    void *Data = nullptr;
    size_t DataSize = 0;

    void* Context = nullptr;
    void* PackContext = nullptr;

    Uint8 *GlyphBuffer = nullptr;
    unsigned GlyphTextureSize = 0;

    unsigned NumChars;

    bool LoadFontSize(float fontSize, unsigned atlasSize);

public:
    FontGlyph* Glyphs = nullptr;

    ISprite* Sprite = nullptr;

    float Size;
    float Ascent;
    float Descent;
    float LineGap;
    int Oversampling;

    unsigned StartChar;
    unsigned EndChar;

    bool LoadFailed;

    Font(const char* filename);

    bool Load(Stream* stream);
    bool LoadFontSize(float fontSize);
    void Dispose();
    ~Font();
};

#endif /* ENGINE_RESOURCETYPES_FONT_H */
