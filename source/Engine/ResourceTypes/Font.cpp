#include <Engine/Diagnostics/Memory.h>
#include <Engine/Graphics.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/Font.h>

// FIXME: Move to another file
#define STBTT_malloc(x, u) Memory::Malloc(x)
#define STBTT_free(x, u) Memory::Free(x)

#define STB_RECT_PACK_IMPLEMENTATION
#include <Libraries/stb_rect_pack.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <Libraries/stb_truetype.h>

Font::Font() {
	LoadFailed = true;

	Init();
}

Font::Font(Stream* stream) {
	LoadFailed = true;

	Init();

	if (Load(stream) && LoadSize(DEFAULT_FONT_SIZE)) {
		LoadFailed = false;
	}
}

Font::Font(std::vector<Stream*> streamList) {
	LoadFailed = true;

	Init();

	if (Load(streamList) && LoadSize(DEFAULT_FONT_SIZE)) {
		LoadFailed = false;
	}
}

void Font::Init() {
	SetOversampling(1);
	UseAntialiasing = true;
	PixelCoverageThreshold = 128;
	FontIndex = 0;
}

bool Font::Load(Stream* stream) {
	FontFamily* family = FontFamily::New(stream);
	if (!family) {
		return false;
	}

	Families.push_back(family);

	return true;
}

bool Font::Load(std::vector<Stream*> streamList) {
	for (size_t i = 0; i < streamList.size(); i++) {
		FontFamily* family = FontFamily::New(streamList[i]);
		if (!family) {
			return false;
		}

		Families.push_back(family);
	}

	return true;
}

FontFamily* FontFamily::New(Stream* stream) {
	size_t dataSize = stream->Length() - stream->Position();
	if (dataSize == 0) {
		return nullptr;
	}

	void* data = (Uint8*)Memory::Malloc(dataSize);
	if (!data) {
		return nullptr;
	}

	stream->ReadBytes(data, dataSize);

	void* context = Memory::Calloc(1, sizeof(stbtt_fontinfo));
	if (context == nullptr) {
		Memory::Free(data);
		return nullptr;
	}

	if (stbtt_InitFont((stbtt_fontinfo*)context, (Uint8*)data, 0) == 0) {
		Memory::Free(data);
		return nullptr;
	}

	FontFamily* family = new FontFamily();
	family->Data = data;
	family->DataSize = dataSize;
	family->Context = context;

	return family;
}

FontFamily::~FontFamily() {
	Memory::Free(Context);
	Memory::Free(Data);
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

void FontGlyphRange::AddGlyph(Uint32 codepoint) {
	// Glyph is already present
	std::unordered_map<Uint32, FontGlyph>::iterator it = Glyphs.find(codepoint);
	if (it != Glyphs.end()) {
		return;
	}

	FontGlyph glyph;
	glyph.Exists = false;
	glyph.FrameID = 0;
	glyph.Codepoint = codepoint;
	Glyphs[codepoint] = glyph;

	NeedUpdate = true;
}

bool FontGlyphRange::Update(Font* font) {
	bool success = true;

	if (NeedUpdate) {
		success = PackGlyphs(font);

		NeedUpdate = false;
	}

	return success;
}

void FontGlyphRange::FreeGlyphMap() {
	for (std::unordered_map<FontFamily*, PackedGlyphs>::iterator it = GlyphsPerFamily.begin();
		it != GlyphsPerFamily.end();
		it++) {
		Memory::Free(GlyphsPerFamily[it->first].PackedChars);
	}

	GlyphsPerFamily.clear();
}

bool FontGlyphRange::InitGlyphMap(Font* font) {
	FreeGlyphMap();

	for (std::unordered_map<Uint32, FontGlyph>::iterator it = Glyphs.begin();
		it != Glyphs.end();
		it++) {
		Uint32 codepoint = it->second.Codepoint;

		FontFamily* family = font->FindFamilyForCodepoint(codepoint);

		GlyphsPerFamily[family].Codepoints.push_back(codepoint);
	}

	for (std::unordered_map<FontFamily*, PackedGlyphs>::iterator it = GlyphsPerFamily.begin();
		it != GlyphsPerFamily.end();
		it++) {
		PackedGlyphs& packedGlyphs = GlyphsPerFamily[it->first];

		void* packedChars =
			Memory::Malloc(packedGlyphs.Codepoints.size() * sizeof(stbtt_packedchar));
		if (packedChars == nullptr) {
			return false;
		}

		packedGlyphs.PackedChars = packedChars;
	}

	return true;
}

bool FontGlyphRange::PackGlyphs(Font* font) {
	stbtt_pack_context* context = (stbtt_pack_context*)Context;
	if (context == nullptr) {
		return false;
	}

	if (!Init()) {
		return false;
	}

	if (!InitGlyphMap(font)) {
		FreeGlyphMap();

		return false;
	}

	stbrp_rect* rects = nullptr;
	size_t lastNumCodepoints = 0;

	bool packedAll = true;

	for (std::unordered_map<FontFamily*, PackedGlyphs>::iterator it = GlyphsPerFamily.begin();
		it != GlyphsPerFamily.end();
		it++) {
		FontFamily* family = it->first;
		PackedGlyphs& packedGlyphs = GlyphsPerFamily[family];

		int* codepoints = packedGlyphs.Codepoints.data();
		size_t numCodepoints = packedGlyphs.Codepoints.size();
		if (numCodepoints == 0) {
			continue;
		}

		stbtt_pack_range range;
		range.first_unicode_codepoint_in_range = 0;
		range.array_of_unicode_codepoints = codepoints;
		range.num_chars = numCodepoints;
		range.chardata_for_range = (stbtt_packedchar*)packedGlyphs.PackedChars;
		range.font_size = FontSize;

		if (numCodepoints > lastNumCodepoints) {
			rects = (stbrp_rect*)Memory::Realloc(
				rects, sizeof(stbrp_rect) * numCodepoints);
			if (rects == nullptr) {
				packedAll = false;
				break;
			}
		}

		stbtt_fontinfo* info = (stbtt_fontinfo*)family->Context;
		if (info == nullptr) {
			packedAll = false;
			break;
		}

		int numRects = stbtt_PackFontRangesGatherRects(context, info, &range, 1, rects);

		stbtt_PackFontRangesPackRects(context, rects, numRects);

		if (stbtt_PackFontRangesRenderIntoRects(context, info, &range, 1, rects) == 0) {
			packedAll = false;
			break;
		}
	}

	Memory::Free(rects);

	if (!packedAll) {
		int maxTextureSize =
			std::min(Graphics::MaxTextureWidth, Graphics::MaxTextureHeight);
		if (AtlasSize > maxTextureSize) {
			return false;
		}

		AtlasSize *= 2;

		return PackGlyphs(font);
	}

	Texture* atlas = Font::CreateAtlasTexture(
		Buffer, AtlasSize, UseAntialiasing, PixelCoverageThreshold);
	if (!atlas) {
		return false;
	}

	if (atlas) {
		Graphics::SetTextureMinFilter(atlas, font->GetAtlasMinFilter());
		Graphics::SetTextureMagFilter(atlas, font->GetAtlasMagFilter());
	}

	Graphics::DisposeTexture(Atlas);

	Atlas = atlas;

	LoadGlyphs();

	return true;
}

void FontGlyphRange::LoadGlyphs() {
	for (std::unordered_map<FontFamily*, PackedGlyphs>::iterator it = GlyphsPerFamily.begin();
		it != GlyphsPerFamily.end();
		it++) {
		PackedGlyphs& packedGlyphs = GlyphsPerFamily[it->first];

		stbtt_packedchar* packedChars = (stbtt_packedchar*)packedGlyphs.PackedChars;
		if (packedChars == nullptr) {
			continue;
		}

		for (size_t i = 0; i < packedGlyphs.Codepoints.size(); i++) {
			int codepoint = packedGlyphs.Codepoints[i];

			stbtt_packedchar* packedChar = &packedChars[i];

			FontGlyph& glyph = Glyphs[codepoint];
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
}

void FontGlyphRange::ReloadAtlas() {
	Uint32* dataRgba =
		Font::GenerateAtlas(Buffer, AtlasSize, UseAntialiasing, PixelCoverageThreshold);
	if (dataRgba) {
		Graphics::UpdateTexture(Atlas, nullptr, dataRgba, AtlasSize * sizeof(Uint32));
		Memory::Free(dataRgba);
	}
}

FontGlyphRange::~FontGlyphRange() {
	FreeGlyphMap();
	if (Context) {
		stbtt_PackEnd((stbtt_pack_context*)Context);
		Memory::Free(Context);
	}
	if (Buffer) {
		Memory::Free(Buffer);
	}
	if (Atlas) {
		Graphics::DisposeTexture(Atlas);
	}
}

FontFamily* Font::FindFamilyForCodepoint(Uint32 codepoint) {
	if (Families.size() == 0) {
		return nullptr;
	}

	for (size_t i = 0; i < Families.size(); i++) {
		FontFamily* family = Families[i];

		int glyphIndex = stbtt_FindGlyphIndex((stbtt_fontinfo*)family->Context, codepoint);
		if (glyphIndex != 0) {
			return family;
		}
	}

	return Families[0];
}

bool Font::LoadSize(float fontSize) {
	if (Families.size() == 0) {
		return false;
	}

	stbtt_fontinfo* fontInfo = (stbtt_fontinfo*)(Families[0]->Context);

	int ascent, descent, lineGap;
	stbtt_GetFontVMetrics(fontInfo, &ascent, &descent, &lineGap);

	ScaleFactor = stbtt_ScaleForPixelHeight(fontInfo, fontSize);

	Ascent = ascent * ScaleFactor;
	Descent = descent * ScaleFactor;
	Leading = lineGap * ScaleFactor;
	SpaceWidth = DEFAULT_FONT_SPACE_WIDTH;
	Size = fontSize;

	Unload();

	InitSprite();
	InitCodepoints();

	FontGlyphRange* range = NewRange();

	for (size_t i = 0; i < Codepoints.size(); i++) {
		range->AddGlyph(Codepoints[i]);
	}

	bool packed = range->PackGlyphs(this);

	LoadGlyphsFromRange(range);

	// Set space width
	const Uint32 spaceCodepoint = 0x0020;
	if (HasGlyph(spaceCodepoint) && IsGlyphLoaded(spaceCodepoint)) {
		SpaceWidth = Glyphs[spaceCodepoint].Advance;
	}

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
		Glyphs[it->first] = it->second;
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

			// Skips empty glyphs (like space characters)
			if (glyph.Width == 0 || glyph.Height == 0) {
				continue;
			}

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
void Font::Update() {
	if (NeedUpdate) {
		for (size_t i = 0; i < GlyphRanges.size(); i++) {
			FontGlyphRange* range = GlyphRanges[i];
			if (range->Update(this)) {
				LoadGlyphsFromRange(range);
			}
		}

		UpdateSprite();

		NeedUpdate = false;
	}
}

void Font::ReloadAtlas() {
	int minFilter = GetAtlasMinFilter();
	int magFilter = GetAtlasMagFilter();

	for (size_t i = 0; i < GlyphRanges.size(); i++) {
		FontGlyphRange* range = GlyphRanges[i];

		range->UseAntialiasing = UseAntialiasing;
		range->PixelCoverageThreshold = PixelCoverageThreshold;

		range->ReloadAtlas();

		Graphics::SetTextureMinFilter(range->Atlas, minFilter);
		Graphics::SetTextureMagFilter(range->Atlas, magFilter);
	}
}

int Font::GetAtlasMinFilter() {
	// TODO: Use TextureFilter_LINEAR_MIPMAP_LINEAR when mipmaps are available.
	return UseAntialiasing ? TextureFilter_LINEAR : TextureFilter_NEAREST;
}
int Font::GetAtlasMagFilter() {
	return UseAntialiasing ? TextureFilter_LINEAR : TextureFilter_NEAREST;
}

bool Font::SetOversampling(int oversamplingValue) {
	Application::Settings->GetInteger("graphics", "forceFontOversampling", &oversamplingValue);

	if (oversamplingValue < 1) {
		oversamplingValue = 1;
	}
	else if (oversamplingValue > 8) {
		oversamplingValue = 8;
	}

	bool didChange = Oversampling != oversamplingValue;

	Oversampling = oversamplingValue;

	return didChange;
}

Uint32* Font::GenerateAtlas(Uint8* data, unsigned size, bool useAntialias, Uint8 threshold) {
	if (!data) {
		return nullptr;
	}

	Uint32* dataRgba = (Uint32*)Memory::Malloc(size * size * sizeof(Uint32));
	if (!dataRgba) {
		return nullptr;
	}

	for (size_t i = 0; i < size * size; i++) {
		Uint8 value = data[i];

		if (!useAntialias) {
			value = (value < threshold) ? 0 : 255;
		}

		dataRgba[i] = (value << 24) | 0xFFFFFF;
	}

	return dataRgba;
}

Texture* Font::CreateAtlasTexture(Uint8* data, unsigned size, bool useAntialias, Uint8 threshold) {
	Uint32* dataRgba = GenerateAtlas(data, size, useAntialias, threshold);
	if (dataRgba == nullptr) {
		return nullptr;
	}

	Texture* atlas =
		Graphics::CreateTextureFromPixels(size, size, dataRgba, size * sizeof(Uint32));

	Memory::Free(dataRgba);

	return atlas;
}

void Font::InitSprite() {
	Sprite = new ISprite();
	Sprite->TakeRef();
	Sprite->AddAnimation("Font", 0, 0);
}

bool Font::IsValidCodepoint(Uint32 codepoint) {
	return codepoint >= 32 && codepoint < 0x110000;
}

FontGlyphRange* Font::FindGlyphInRange(Uint32 codepoint) {
	size_t numRanges = GlyphRanges.size();
	if (numRanges == 0) {
		return nullptr;
	}

	FontGlyphRange* range = GlyphRanges[numRanges - 1];
	std::unordered_map<Uint32, FontGlyph>::iterator it = range->Glyphs.find(codepoint);
	if (it != range->Glyphs.end()) {
		return range;
	}

	return nullptr;
}

FontGlyphRange* Font::NewRange() {
	FontGlyphRange* range = new FontGlyphRange(GlyphRanges.size());
	range->FontSize = Size;
	range->FontIndex = FontIndex;
	range->Oversampling = Oversampling;
	range->UseAntialiasing = UseAntialiasing;
	range->PixelCoverageThreshold = PixelCoverageThreshold;
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

bool Font::HasGlyph(Uint32 codepoint) {
	if (!IsValidCodepoint(codepoint)) {
		return false;
	}

	return FindFamilyForCodepoint(codepoint) != nullptr;
}

bool Font::IsGlyphLoaded(Uint32 codepoint) {
	if (IsValidCodepoint(codepoint)) {
		std::unordered_map<Uint32, FontGlyph>::iterator it = Glyphs.find(codepoint);
		if (it != Glyphs.end()) {
			return it->second.Exists;
		}
	}

	return false;
}

bool Font::RequestGlyph(Uint32 codepoint) {
	std::unordered_map<Uint32, FontGlyph>::iterator it = Glyphs.find(codepoint);
	if (it != Glyphs.end()) {
		// Glyph is already present in the font
		return it->second.Exists;
	}

	// Add codepoint
	Codepoints.push_back((int)codepoint);

	// Check if a given range has this glyph
	FontGlyphRange* range = FindGlyphInRange(codepoint);
	if (range) {
		// Copy it if so
		Glyphs[codepoint] = range->Glyphs[codepoint];

		return true;
	}

	// Add to last range or create a new one
	range = GetRangeForNewGlyph();
	range->AddGlyph(codepoint);

	NeedUpdate = true;

	return true;
}

float Font::GetGlyphAdvance(Uint32 codepoint) {
	if (IsValidCodepoint(codepoint)) {
		if (IsGlyphLoaded(codepoint)) {
			std::unordered_map<Uint32, FontGlyph>::iterator it = Glyphs.find(codepoint);
			if (it != Glyphs.end()) {
				return it->second.Advance;
			}
		}

		FontFamily* family = FindFamilyForCodepoint(codepoint);
		if (family) {
			stbtt_fontinfo* info = (stbtt_fontinfo*)family->Context;

			int advanceWidth = 0;
			stbtt_GetGlyphHMetrics(info, stbtt_FindGlyphIndex(info, codepoint), &advanceWidth, nullptr);

			return (float)advanceWidth * stbtt_ScaleForPixelHeight(info, Size);
		}
	}

	return 0.0f;
}

float Font::GetEllipsisWidth() {
	if (HasGlyph(ELLIPSIS_CODE_POINT) && IsGlyphLoaded(ELLIPSIS_CODE_POINT)) {
		return Glyphs[ELLIPSIS_CODE_POINT].Advance;
	}
	else if (HasGlyph(FULL_STOP_CODE_POINT) && IsGlyphLoaded(FULL_STOP_CODE_POINT)) {
		return Glyphs[FULL_STOP_CODE_POINT].Advance * 3;
	}

	return 0.0f;
}

void Font::Unload() {
	for (size_t i = 0; i < GlyphRanges.size(); i++) {
		delete GlyphRanges[i];
	}
	GlyphRanges.clear();

	if (Sprite) {
		Sprite->Release();
		Sprite = nullptr;
	}
}

void Font::Dispose() {
	Unload();

	for (size_t i = 0; i < Families.size(); i++) {
		delete Families[i];
	}
}

Font::~Font() {
	Dispose();
}
