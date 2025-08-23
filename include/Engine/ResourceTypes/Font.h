#ifndef ENGINE_RESOURCETYPES_FONT_H
#define ENGINE_RESOURCETYPES_FONT_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/ISprite.h>

#define DEFAULT_FONT_SIZE 40
#define DEFAULT_FONT_ATLAS_SIZE 128
#define MAX_FONT_ATLAS_SIZE 2048

struct FontGlyph {
	bool Exists;
	int Codepoint;
	int FrameID;
	unsigned Width;
	unsigned Height;
	unsigned SourceX;
	unsigned SourceY;
	float OffsetX;
	float OffsetY;
	float Advance;
};

// Each glyph range corresponds to a font atlas, which cannot exceed MAX_FONT_ATLAS_SIZE in size.
struct FontGlyphRange {
private:
	void* Context = nullptr;

	void* FontData = nullptr;
	float FontSize;

	Uint8* Buffer = nullptr;

	bool NeedUpdate = false;

	size_t InitCodepointList();
	void LoadGlyphs();

public:
	unsigned ID = 0;
	unsigned AtlasSize = 0;
	int Oversampling = 1;
	int FontIndex = 0;

	std::vector<int> Codepoints;
	std::unordered_map<Uint32, FontGlyph> Glyphs;
	std::unordered_map<Uint32, Uint32> CodepointToGlyphIndex;

	void* PackedChars = nullptr;

	Texture* Atlas = nullptr;

	bool Init();
	bool PackGlyphs();
	bool Refresh();
	void SetFontData(void* fontData, float fontSize);
	void AddGlyph(void* fontInfo, Uint32 codepoint);
	void AddGlyph(Uint32 codepoint, Uint32 glyphIndex);

	FontGlyphRange(unsigned id);
	~FontGlyphRange();
};

class Font {
private:
	void* Data = nullptr;
	size_t DataSize = 0;

	void* Context = nullptr;

	std::vector<FontGlyphRange*> GlyphRanges;

	bool NeedUpdate = false;

	void InitCodepoints();
	FontGlyphRange* FindGlyphInRange(Uint32 glyphIndex);
	FontGlyphRange* NewRange();
	FontGlyphRange* GetRangeForNewGlyph();
	void LoadGlyphsFromRange(FontGlyphRange* range);
	void InitSprite();
	void UpdateSprite();
	void Unload();

public:
	std::vector<int> Codepoints;
	std::unordered_map<Uint32, FontGlyph> Glyphs;
	std::unordered_map<Uint32, Uint32> CodepointToGlyphIndex;

	ISprite* Sprite = nullptr;

	float Size;
	float Ascent;
	float Descent;
	float LineGap;
	int Oversampling;
	int FontIndex;

	bool LoadFailed;

	Font(const char* filename);

	bool Load(Stream* stream);
	bool LoadSize(float fontSize);
	bool Reload();

	bool IsValidCodepoint(Uint32 codepoint);
	bool RequestGlyph(Uint32 codepoint);
	Uint32 GetGlyphIndex(Uint32 codepoint);
	void Refresh();

	static Texture* CreateAtlas(Uint8* data, unsigned size);

	void Dispose();
	~Font();
};

#endif /* ENGINE_RESOURCETYPES_FONT_H */
