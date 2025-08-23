#include <Engine/Diagnostics/Memory.h>
#include <Engine/Graphics.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/Font.h>

#define STBTT_malloc(x, u) Memory::Malloc(x)
#define STBTT_free(x, u) Memory::Free(x)

#define STB_RECT_PACK_IMPLEMENTATION
#include <Libraries/stb_rect_pack.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <Libraries/stb_truetype.h>

Font::Font(const char* filename) {
	LoadFailed = true;

	Stream* stream = ResourceStream::New(filename);
	if (stream) {
		Load(stream);
		stream->Close();
	}
}

bool Font::Load(Stream* stream) {
	// stb_truetype needs this data allocated for as long as it's used, so it can only be freed
	// when the Font itself is.
	DataSize = stream->Length();
	Data = (Uint8*)Memory::Malloc(DataSize);
	if (!Data) {
		return false;
	}

	stream->ReadBytes(Data, DataSize);

	Context = Memory::Calloc(1, sizeof(stbtt_fontinfo));
	Oversampling = 1;
	FontIndex = 0;

	// Load font
	if (stbtt_InitFont((stbtt_fontinfo*)Context, (Uint8*)Data, 0) == 0) {
		return false;
	}

	if (LoadSize(DEFAULT_FONT_SIZE)) {
		LoadFailed = false;
	}

	return !LoadFailed;
}

void Font::InitCodepoints() {
	Codepoints.clear();

	// Load Basic Latin codepoints (except C0 controls)
	// U+0020 to U+007E
	for (int i = 32; i < 127; i++) {
		Codepoints.push_back(i);
	}
}

FontGlyphRange::FontGlyphRange(unsigned id) {
	ID = id;
	AtlasSize = DEFAULT_FONT_ATLAS_SIZE;
}

void FontGlyphRange::SetFontData(void* fontData, float fontSize) {
	FontData = fontData;
	FontSize = fontSize;
}

bool FontGlyphRange::Init() {
	Buffer = (Uint8*)Memory::Realloc(Buffer, AtlasSize * AtlasSize);
	if (Buffer == nullptr) {
		return false;
	}

	// Init packing if needed
	stbtt_pack_context* context = (stbtt_pack_context*)Context;
	if (context == nullptr) {
		Context = Memory::Calloc(1, sizeof(stbtt_pack_context));
		context = (stbtt_pack_context*)Context;
		if (context == nullptr) {
			return false;
		}
	}
	else {
		stbtt_PackEnd(context);
	}

	stbtt_PackBegin(context, Buffer, AtlasSize, AtlasSize, AtlasSize, 1, nullptr);
	stbtt_PackSetSkipMissingCodepoints(context, false);
	stbtt_PackSetOversampling(context, Oversampling, Oversampling);

	return true;
}

void FontGlyphRange::AddGlyph(void* fontInfo, Uint32 codepoint) {
	Uint32 glyphIndex = stbtt_FindGlyphIndex((stbtt_fontinfo*)fontInfo, codepoint);

	// Glyph is already present
	std::unordered_map<Uint32, FontGlyph>::iterator it = Glyphs.find(glyphIndex);
	if (it != Glyphs.end()) {
		return;
	}

	AddGlyph(codepoint, glyphIndex);
}

void FontGlyphRange::AddGlyph(Uint32 codepoint, Uint32 glyphIndex) {
	FontGlyph glyph;
	glyph.Exists = false;
	glyph.FrameID = 0;
	glyph.Codepoint = codepoint;
	Glyphs[glyphIndex] = glyph;
	CodepointToGlyphIndex[codepoint] = glyphIndex;

	NeedUpdate = true;
}

bool FontGlyphRange::Refresh() {
	bool success = true;

	if (NeedUpdate) {
		success = PackGlyphs();

		NeedUpdate = false;
	}

	return success;
}

size_t FontGlyphRange::InitCodepointList() {
	// This is a little unnecessary, since stb_truetype will just get the glyphs from the codepoints.
	// TODO: Reimplement stb_truetype's packing so that this is not necessary
	Codepoints.clear();

	for (std::unordered_map<Uint32, FontGlyph>::iterator it = Glyphs.begin();
		it != Glyphs.end();
		it++) {
		Codepoints.push_back(it->second.Codepoint);
	}

	return Codepoints.size();
}

bool FontGlyphRange::PackGlyphs() {
	stbtt_pack_context* context = (stbtt_pack_context*)Context;
	if (context == nullptr) {
		return false;
	}

	if (!Init()) {
		return false;
	}

	size_t numCodepoints = InitCodepointList();

	PackedChars = Memory::Realloc(PackedChars, numCodepoints * sizeof(stbtt_packedchar));
	if (PackedChars == nullptr) {
		return false;
	}

	stbtt_packedchar* packedChars = (stbtt_packedchar*)PackedChars;

	stbtt_pack_range range;
	range.first_unicode_codepoint_in_range = 0;
	range.array_of_unicode_codepoints = Codepoints.data();
	range.num_chars = numCodepoints;
	range.chardata_for_range = packedChars;
	range.font_size = FontSize;

	if (stbtt_PackFontRanges(context, (Uint8*)FontData, FontIndex, &range, 1) == 0) {
		int maxTextureSize =
			std::min(Graphics::MaxTextureWidth, Graphics::MaxTextureHeight);
		if (AtlasSize > maxTextureSize) {
			return false;
		}

		AtlasSize *= 2;

		return PackGlyphs();
	}

	Texture* atlas = Font::CreateAtlas(Buffer, AtlasSize);
	if (!atlas) {
		return false;
	}

	Graphics::DisposeTexture(Atlas);

	Atlas = atlas;

	LoadGlyphs();

	return true;
}

void FontGlyphRange::LoadGlyphs() {
	stbtt_packedchar* packedChars = (stbtt_packedchar*)PackedChars;
	if (packedChars == nullptr) {
		return;
	}

	for (size_t i = 0; i < Codepoints.size(); i++) {
		int codepoint = Codepoints[i];

		stbtt_packedchar* packedChar = &packedChars[i];

		Uint32 glyphIndex = CodepointToGlyphIndex[codepoint];

		FontGlyph& glyph = Glyphs[glyphIndex];
		glyph.Exists = true;
		glyph.Width = packedChar->x1 - packedChar->x0;
		glyph.Height = packedChar->y1 - packedChar->y0;
		glyph.SourceX = packedChar->x0;
		glyph.SourceY = packedChar->y0;
		glyph.OffsetX = packedChar->xoff * Oversampling;
		glyph.OffsetY = packedChar->yoff * Oversampling;
		glyph.Advance = packedChar->xadvance;
	}
}

FontGlyphRange::~FontGlyphRange() {
	if (Context) {
		stbtt_PackEnd((stbtt_pack_context*)Context);
		Memory::Free(Context);
	}
	if (PackedChars) {
		Memory::Free(PackedChars);
	}
	if (Buffer) {
		Memory::Free(Buffer);
	}
	if (Atlas) {
		Graphics::DisposeTexture(Atlas);
	}
}

bool Font::LoadSize(float fontSize) {
	int ascent, descent, lineGap;
	float scale = stbtt_ScaleForPixelHeight((stbtt_fontinfo*)Context, fontSize);
	stbtt_GetFontVMetrics((stbtt_fontinfo*)Context, &ascent, &descent, &lineGap);

	Ascent = ascent * scale;
	Descent = descent * scale;
	LineGap = lineGap * scale;
	Size = fontSize;

	Unload();

	InitSprite();
	InitCodepoints();

	FontGlyphRange* range = NewRange();

	for (size_t i = 0; i < Codepoints.size(); i++) {
		range->AddGlyph(Context, Codepoints[i]);
	}

	bool packed = range->PackGlyphs();

	LoadGlyphsFromRange(range);

	if (packed) {
		UpdateSprite();

		return true;
	}

	return false;
}

bool Font::Reload() {
	return LoadSize(Size);
}

void Font::LoadGlyphsFromRange(FontGlyphRange* range) {
	for (std::unordered_map<Uint32, FontGlyph>::iterator it = range->Glyphs.begin();
		it != range->Glyphs.end();
		it++) {
		Uint32 glyphIndex = it->first;

		Glyphs[glyphIndex] = range->Glyphs[glyphIndex];

		CodepointToGlyphIndex[it->second.Codepoint] = glyphIndex;
	}
}

void Font::UpdateSprite() {
	if (!Sprite) {
		return;
	}

	Sprite->Spritesheets.clear();
	Sprite->SpritesheetFilenames.clear();
	Sprite->Animations[0].Frames.clear();

	for (size_t i = 0; i < GlyphRanges.size(); i++) {
		FontGlyphRange* range = GlyphRanges[i];
		if (range->Atlas == nullptr) {
			continue;
		}

		Sprite->Spritesheets.push_back(range->Atlas);
		Sprite->SpritesheetFilenames.push_back("");

		for (std::unordered_map<Uint32, FontGlyph>::iterator it = range->Glyphs.begin();
			it != range->Glyphs.end();
			it++) {
			FontGlyph& glyph = Glyphs[it->first];

			glyph.FrameID = Sprite->Animations[0].Frames.size();

			// Offsets are handled when rendering
			Sprite->AddFrame(0,
				0,
				glyph.SourceX,
				glyph.SourceY,
				glyph.Width,
				glyph.Height,
				0,
				0,
				0,
				range->ID);
		}
	}

	Sprite->RefreshGraphicsID();
}

// Font updating is done lazily, which helps if multiple glyphs needed to be loaded at once.
void Font::Refresh() {
	if (NeedUpdate) {
		for (size_t i = 0; i < GlyphRanges.size(); i++) {
			FontGlyphRange* range = GlyphRanges[i];
			if (range->Refresh()) {
				LoadGlyphsFromRange(range);
			}
		}

		UpdateSprite();

		NeedUpdate = false;
	}
}

Texture* Font::CreateAtlas(Uint8* data, unsigned size) {
	// Convert texture to RGBA
	Uint32* dataRgba = (Uint32*)Memory::Malloc(size * size * sizeof(Uint32));
	if (!dataRgba) {
		return nullptr;
	}

	for (size_t i = 0; i < size * size; i++) {
		// No reordering needed because alpha is always first!
		dataRgba[i] = (data[i] << 24) | 0xFFFFFF;
	}

	// TODO: A better way to toggle texture interpolation.
	bool original = Graphics::TextureInterpolate;
	Graphics::SetTextureInterpolation(true);

	// This may return nullptr.
	Texture* atlas =
		Graphics::CreateTextureFromPixels(size, size, dataRgba, size * sizeof(Uint32));

	Graphics::SetTextureInterpolation(original);

	Memory::Free(dataRgba);

	return atlas;
}

void Font::InitSprite() {
	Sprite = new ISprite();
	Sprite->Filename = StringUtils::Duplicate("Font Sprite");
	Sprite->AddAnimation("Font", 0, 0);
}

bool Font::IsValidCodepoint(Uint32 codepoint) {
	return codepoint >= 32 && codepoint < 0x110000;
}

FontGlyphRange* Font::FindGlyphInRange(Uint32 glyphIndex) {
	size_t numRanges = GlyphRanges.size();
	if (numRanges == 0) {
		return nullptr;
	}

	FontGlyphRange* range = GlyphRanges[numRanges - 1];
	std::unordered_map<Uint32, FontGlyph>::iterator it = range->Glyphs.find(glyphIndex);
	if (it != range->Glyphs.end()) {
		return range;
	}

	return nullptr;
}

FontGlyphRange* Font::NewRange() {
	FontGlyphRange* range = new FontGlyphRange(GlyphRanges.size());
	range->SetFontData(Data, Size);
	range->Oversampling = Oversampling;
	range->FontIndex = FontIndex;
	range->Init();

	GlyphRanges.push_back(range);

	return range;
}

FontGlyphRange* Font::GetRangeForNewGlyph() {
	size_t numRanges = GlyphRanges.size();
	if (numRanges > 0) {
		FontGlyphRange* range = GlyphRanges[numRanges - 1];

		// Check if the texture is too large already
		int maxTextureSize =
			std::min(Graphics::MaxTextureWidth, Graphics::MaxTextureHeight);
		maxTextureSize = std::min(maxTextureSize, MAX_FONT_ATLAS_SIZE);
		if (range->AtlasSize < maxTextureSize) {
			return range;
		}
	}

	return NewRange();
}

bool Font::RequestGlyph(Uint32 codepoint) {
	std::unordered_map<Uint32, Uint32>::iterator it = CodepointToGlyphIndex.find(codepoint);
	if (it != CodepointToGlyphIndex.end()) {
		// Glyph is already present in the font
		return Glyphs[it->second].Exists;
	}

	// Add codepoint
	Codepoints.push_back((int)codepoint);

	// Get glyph index
	Uint32 glyphIndex = stbtt_FindGlyphIndex((stbtt_fontinfo*)Context, codepoint);

	// Check if a given range has this glyph
	FontGlyphRange* range = FindGlyphInRange(glyphIndex);
	if (range) {
		// Copy it if so
		Glyphs[glyphIndex] = range->Glyphs[glyphIndex];
		CodepointToGlyphIndex[codepoint] = glyphIndex;

		return true;
	}

	// Add to last range or create a new one
	range = GetRangeForNewGlyph();
	range->AddGlyph(codepoint, glyphIndex);

	NeedUpdate = true;

	return true;
}

Uint32 Font::GetGlyphIndex(Uint32 codepoint) {
	std::unordered_map<Uint32, Uint32>::iterator it = CodepointToGlyphIndex.find(codepoint);
	if (it != CodepointToGlyphIndex.end()) {
		return it->second;
	}
	return 0;
}

void Font::Unload() {
	for (size_t i = 0; i < GlyphRanges.size(); i++) {
		delete GlyphRanges[i];
	}
	GlyphRanges.clear();

	if (Sprite) {
		delete Sprite;
		Sprite = nullptr;
	}
}

void Font::Dispose() {
	Unload();

	if (Context) {
		Memory::Free(Context);
		Context = nullptr;
	}
	if (Data) {
		Memory::Free(Data);
		Data = nullptr;
	}
}

Font::~Font() {
	Dispose();
}
