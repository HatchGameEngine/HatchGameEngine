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

	InitCodepoints();

	// Load font
	if (stbtt_InitFont((stbtt_fontinfo*)Context, (Uint8*)Data, 0) == 0) {
		return false;
	}

	if (LoadFontSize(DEFAULT_FONT_SIZE)) {
		LoadFailed = false;
	}

	return !LoadFailed;
}

void Font::InitCodepoints() {
	// Load Basic Latin codepoints (except C0 controls)
	// U+0020 to U+007F
	for (int i = 32; i < 128; i++) {
		Codepoints.push_back(i);
	}
}

bool Font::LoadFontSize(float fontSize, unsigned atlasSize) {
	int ascent, descent, lineGap;
	stbtt_fontinfo* fontInfo = (stbtt_fontinfo*)Context;

	float scale = stbtt_ScaleForPixelHeight(fontInfo, fontSize);
	stbtt_GetFontVMetrics(fontInfo, &ascent, &descent, &lineGap);

	Ascent = ascent * scale;
	Descent = descent * scale;
	LineGap = lineGap * scale;

	// Allocate the grayscale buffer
	if (atlasSize == 0) {
		atlasSize = DEFAULT_FONT_ATLAS_SIZE;
	}

	if (atlasSize > GlyphTextureSize || GlyphBuffer == nullptr) {
		GlyphBuffer = (Uint8*)Memory::Realloc(GlyphBuffer, atlasSize * atlasSize);
	}

	if (!GlyphBuffer) {
		return false;
	}

	// Init packing if needed
	stbtt_pack_context* packContext = (stbtt_pack_context*)PackContext;
	if (packContext == nullptr) {
		PackContext = Memory::Calloc(1, sizeof(stbtt_pack_context));
		packContext = (stbtt_pack_context*)PackContext;
	}
	else {
		stbtt_PackEnd(packContext);
	}

	unsigned width = atlasSize;
	unsigned height = atlasSize;

	stbtt_PackBegin(packContext, GlyphBuffer, width, height, width, 1, nullptr);
	stbtt_PackSetSkipMissingCodepoints(packContext, false);
	stbtt_PackSetOversampling(packContext, Oversampling, Oversampling);

	// Pack glyphs
	unsigned numCodepoints = Codepoints.size();
	if (numCodepoints == 0) {
		return false;
	}

	stbtt_packedchar* packedChars =
		(stbtt_packedchar*)Memory::Calloc(numCodepoints, sizeof(stbtt_packedchar));
	if (!packedChars) {
		return false;
	}

	stbtt_pack_range range;
	range.first_unicode_codepoint_in_range = 0;
	range.array_of_unicode_codepoints = Codepoints.data();
	range.num_chars = numCodepoints;
	range.chardata_for_range = packedChars;
	range.font_size = fontSize;

	if (stbtt_PackFontRanges(packContext, (Uint8*)Data, 0, &range, 1) == 0) {
		// Didn't work... So we try to increase the size of the texture atlas
		// But it can't exceed the max texture size of the GPU.
		int maxTextureSize =
			std::min(Graphics::MaxTextureWidth, Graphics::MaxTextureHeight);
		if (atlasSize >= maxTextureSize) {
			Memory::Free(packedChars);
			return false;
		}

		atlasSize *= 2;

		return LoadFontSize(fontSize, atlasSize);
	}

	// Load the glyphs
	for (size_t i = 0; i < numCodepoints; i++) {
		int codepoint = Codepoints[i];
		stbtt_packedchar* packedChar = &packedChars[i];

		FontGlyph glyph;
		glyph.Codepoint = codepoint;
		glyph.AtlasID = 0;
		glyph.Width = packedChar->x1 - packedChar->x0;
		glyph.Height = packedChar->y1 - packedChar->y0;
		glyph.SourceX = packedChar->x0;
		glyph.SourceY = packedChar->y0;
		glyph.OffsetX = packedChar->xoff * Oversampling;
		glyph.OffsetY = packedChar->yoff * Oversampling;
		glyph.Advance = packedChar->xadvance;

		Glyphs[codepoint] = glyph;
	}

	Memory::Free(packedChars);

	// Create the texture atlas
	// Eventually, this may need to handle multiple textures. For now, there is only one atlas.
	if (!CreateTextureAtlas(width, height)) {
		return false;
	}

	// Create the sprite
	if (!LoadSprite()) {
		return false;
	}

	GlyphTextureSize = atlasSize;
	Size = fontSize;

	return true;
}

bool Font::CreateTextureAtlas(unsigned width, unsigned height) {
	// Convert texture to RGBA
	Uint32* textureData = (Uint32*)Memory::Malloc(width * height * sizeof(Uint32));
	if (!textureData) {
		return false;
	}

	for (size_t i = 0; i < width * height; i++) {
		// No reordering needed because alpha is always first!
		textureData[i] = (GlyphBuffer[i] << 24) | 0xFFFFFF;
	}

	if (Atlas) {
		Graphics::DisposeTexture(Atlas);
		Atlas = nullptr;
	}

	// TODO: A better way to toggle texture interpolation.
	bool original = Graphics::TextureInterpolate;
	Graphics::SetTextureInterpolation(true);

	Atlas = Graphics::CreateTextureFromPixels(
		width, height, textureData, width * sizeof(Uint32));

	Graphics::SetTextureInterpolation(original);

	Memory::Free(textureData);

	return Atlas != nullptr;
}

bool Font::LoadSprite() {
	// TODO: No need to delete the entire sprite in most cases, just update it
	if (Sprite) {
		delete Sprite;
	}

	Sprite = new ISprite();
	Sprite->Filename = StringUtils::Duplicate("Font Sprite");
	Sprite->AddAnimation("Font", 0, 0);
	Sprite->Spritesheets.push_back(Atlas);
	Sprite->SpritesheetFilenames.push_back("");

	// Add the glyphs to the sprite
	for (size_t i = 0; i < Codepoints.size(); i++) {
		FontGlyph& glyph = Glyphs[Codepoints[i]];

		glyph.FrameID = i;

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
			0);
	}

	Sprite->RefreshGraphicsID();

	return true;
}

bool Font::LoadFontSize(float fontSize) {
	return LoadFontSize(fontSize, 0);
}

bool Font::IsValidCodepoint(Uint32 codepoint) {
	return codepoint >= 32 && codepoint < 0x110000;
}

bool Font::IsGlyphLoaded(Uint32 codepoint) {
	auto it = std::find(Codepoints.begin(), Codepoints.end(), (int)codepoint);
	return it != Codepoints.end();
}

bool Font::RequestGlyph(Uint32 codepoint) {
	if (IsGlyphLoaded(codepoint)) {
		return true;
	}

	Codepoints.push_back((int)codepoint);

	return LoadFontSize(Size, GlyphTextureSize);
}

void Font::Dispose() {
	if (Context) {
		Memory::Free(Context);
		Context = nullptr;
	}
	if (PackContext) {
		stbtt_PackEnd((stbtt_pack_context*)PackContext);
		Memory::Free(PackContext);
		PackContext = nullptr;
	}
	if (GlyphBuffer) {
		Memory::Free(GlyphBuffer);
		GlyphBuffer = nullptr;
	}
	if (Atlas) {
		Graphics::DisposeTexture(Atlas);
		Atlas = nullptr;
	}
	if (Sprite) {
		delete Sprite;
		Sprite = nullptr;
	}
	if (Data) {
		Memory::Free(Data);
		Data = nullptr;
	}
}

Font::~Font() {
	Dispose();
}
