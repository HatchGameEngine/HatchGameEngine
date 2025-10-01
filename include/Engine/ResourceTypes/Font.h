#ifndef ENGINE_RESOURCETYPES_FONT_H
#define ENGINE_RESOURCETYPES_FONT_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/ISprite.h>

#define DEFAULT_FONT_SIZE 40
#define DEFAULT_FONT_SPACE_WIDTH 6.0
#define DEFAULT_FONT_ATLAS_SIZE 128
#define MAX_FONT_ATLAS_SIZE 2048

#define FULL_STOP_CODE_POINT 0x002E
#define ELLIPSIS_CODE_POINT 0x2026

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

class FontFamily {
public:
	void* Data = nullptr;
	size_t DataSize = 0;

	void* Context = nullptr;

	static FontFamily* New(Stream* stream);

	~FontFamily();
};

struct PackedGlyphs {
	std::vector<int> Codepoints;

	void* PackedChars = nullptr;
};

class Font;

// Each glyph range uniquely corresponds to a font atlas
struct FontGlyphRange {
private:
	void* Context = nullptr;
	Uint8* Buffer = nullptr;

	bool NeedUpdate = false;

	bool InitGlyphMap(Font* font);
	void FreeGlyphMap();
	void LoadGlyphs();

public:
	unsigned ID = 0;
	unsigned AtlasSize = 0;

	float FontSize;
	int FontIndex;

	int Oversampling;
	bool UseAntialiasing;
	Uint8 PixelCoverageThreshold;

	std::unordered_map<FontFamily*, PackedGlyphs> GlyphsPerFamily;
	std::unordered_map<Uint32, FontGlyph> Glyphs;

	void* PackedChars = nullptr;
	Texture* Atlas = nullptr;

	bool Init();
	bool PackGlyphs(Font* font);
	bool Update(Font* font);
	void ReloadAtlas();
	void AddGlyph(Uint32 codepoint);

	FontGlyphRange(unsigned id);
	~FontGlyphRange();
};

class Font {
private:
	std::vector<FontFamily*> Families;
	std::vector<FontGlyphRange*> GlyphRanges;

	bool NeedUpdate = false;

	void InitCodepoints();

	FontGlyphRange* FindGlyphInRange(Uint32 codepoint);
	FontGlyphRange* NewRange();
	FontGlyphRange* GetRangeForNewGlyph();

	void LoadGlyphsFromRange(FontGlyphRange* range);

	bool IsGlyphLoaded(Uint32 codepoint);

	void InitSprite();
	void UpdateSprite();
	void Unload();

public:
	std::vector<int> Codepoints;
	std::unordered_map<Uint32, FontGlyph> Glyphs;

	ISprite* Sprite = nullptr;

	float Size;
	float Ascent;
	float Descent;
	float Leading;
	float SpaceWidth;
	float ScaleFactor;
	int Oversampling;
	bool UseAntialiasing;
	Uint8 PixelCoverageThreshold;

	int FontIndex;

	bool LoadFailed;

	Font();
	Font(Stream* stream);
	Font(std::vector<Stream*> streamList);

	void Init();
	bool Load(Stream* stream);
	bool Load(std::vector<Stream*> streamList);
	bool LoadSize(float fontSize);
	bool Reload();
	void ReloadAtlas();

	int GetAtlasMinFilter();
	int GetAtlasMagFilter();

	bool IsValidCodepoint(Uint32 codepoint);
	bool HasGlyph(Uint32 codepoint);
	bool RequestGlyph(Uint32 codepoint);
	float GetGlyphAdvance(Uint32 codepoint);
	float GetEllipsisWidth();
	void Update();

	FontFamily* FindFamilyForCodepoint(Uint32 codepoint);

	static Uint32*
	GenerateAtlas(Uint8* data, unsigned size, bool useAntialias, Uint8 threshold);
	static Texture*
	CreateAtlasTexture(Uint8* data, unsigned size, bool useAntialias, Uint8 threshold);

	void Dispose();
	~Font();
};

#endif /* ENGINE_RESOURCETYPES_FONT_H */
