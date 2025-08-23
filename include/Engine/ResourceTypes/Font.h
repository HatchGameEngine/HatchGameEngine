#ifndef ENGINE_RESOURCETYPES_FONT_H
#define ENGINE_RESOURCETYPES_FONT_H

#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/ISprite.h>

#define DEFAULT_FONT_SIZE 40
#define DEFAULT_FONT_ATLAS_SIZE 128

struct FontGlyph {
    int Codepoint;
    int AtlasID;
    int FrameID;
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

    Texture* Atlas = nullptr;
    Uint8 *GlyphBuffer = nullptr;
    unsigned GlyphTextureSize = 0;

    void InitCodepoints();

    bool LoadFontSize(float fontSize, unsigned atlasSize);
    bool CreateTextureAtlas(unsigned width, unsigned height);
    bool LoadSprite();

public:
    std::vector<int> Codepoints;
    std::unordered_map<Uint32, FontGlyph> Glyphs;

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

    bool IsValidCodepoint(Uint32 codepoint);
    bool IsGlyphLoaded(Uint32 codepoint);
    bool RequestGlyph(Uint32 codepoint);

    void Dispose();
    ~Font();
};

#endif /* ENGINE_RESOURCETYPES_FONT_H */
