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
	StartChar = 32;
	EndChar = 127;

	// Load font
	// FIXME: I don't think using stbtt_GetFontOffsetForIndex is necessary, since Hatch doesn't
	// have a way of handling font collections...
	int fontOffset = stbtt_GetFontOffsetForIndex((Uint8*)Data, 0);
	if (stbtt_InitFont((stbtt_fontinfo*)Context, (Uint8*)Data, fontOffset) == 0) {
		return false;
	}

	if (LoadFontSize(40)) {
		LoadFailed = false;
	}

	return !LoadFailed;
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
		atlasSize = 128;
	}

	if (atlasSize > GlyphTextureSize || GlyphBuffer == nullptr) {
		GlyphBuffer = (Uint8*)Memory::Realloc(GlyphBuffer, atlasSize * atlasSize);
	}

	if (!GlyphBuffer) {
		return false;
	}

	GlyphTextureSize = atlasSize;

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
	unsigned numCharsInRange = (EndChar - StartChar) + 1;

	stbtt_packedchar* packedChars =
		(stbtt_packedchar*)Memory::Calloc(numCharsInRange, sizeof(stbtt_packedchar));
	if (!packedChars) {
		return false;
	}

	if (stbtt_PackFontRange(packContext,
		    (Uint8*)Data,
		    0,
		    fontSize,
		    StartChar,
		    numCharsInRange,
		    packedChars) == 0) {
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

	Glyphs = (FontGlyph*)Memory::Realloc(Glyphs, numCharsInRange * sizeof(FontGlyph));
	if (!Glyphs) {
		return false;
	}

	// Load the glyphs
	for (unsigned i = 0; i < numCharsInRange; i++) {
		FontGlyph* glyph = &Glyphs[i];
		stbtt_packedchar* packedChar = &packedChars[i];

		glyph->Width = packedChar->x1 - packedChar->x0;
		glyph->Height = packedChar->y1 - packedChar->y0;
		glyph->SourceX = packedChar->x0;
		glyph->SourceY = packedChar->y0;
		glyph->OffsetX = packedChar->xoff * Oversampling;
		glyph->OffsetY = packedChar->yoff * Oversampling;
		glyph->Advance = packedChar->xadvance;
	}

	Memory::Free(packedChars);

	// Convert texture to RGBA
	Uint32* textureData = (Uint32*)Memory::Malloc(width * height * sizeof(Uint32));
	if (!textureData) {
		return false;
	}

	for (size_t i = 0; i < width * height; i++) {
		// No reordering needed because alpha is always first!
		textureData[i] = (GlyphBuffer[i] << 24) | 0xFFFFFF;
	}

	// Create the texture
	// TODO: A better way to define texture interpolation.
	bool original = Graphics::TextureInterpolate;
	Graphics::SetTextureInterpolation(true);

	Texture* textureAtlas = Graphics::CreateTextureFromPixels(
		width, height, textureData, width * sizeof(Uint32));

	Memory::Free(textureData);

	Graphics::SetTextureInterpolation(original);

	if (!textureAtlas) {
		return false;
	}

	// Delete the previous sprite, if it exists
	if (Sprite) {
		delete Sprite;
	}

	// Create the sprite
	Sprite = new ISprite();
	Sprite->Filename = StringUtils::Duplicate("Font Sprite");
	Sprite->AddAnimation("Font", 0, 0);
	Sprite->Spritesheets.push_back(textureAtlas);

	// We own this spritesheet, so we don't name it
	// TODO: Find a better way to do this
	Sprite->SpritesheetFilenames.push_back("");

	// Add the glyphs to the sprite
	NumChars = numCharsInRange;

	for (unsigned i = 0; i < NumChars; i++) {
		FontGlyph* glyph = &Glyphs[i];

		// Offsets are handled when rendering
		Sprite->AddFrame(0,
			0,
			glyph->SourceX,
			glyph->SourceY,
			glyph->Width,
			glyph->Height,
			0,
			0,
			0,
			0);
	}

	Sprite->RefreshGraphicsID();

	Size = fontSize;

	return true;
}

bool Font::LoadFontSize(float fontSize) {
	return LoadFontSize(fontSize, 0);
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
	if (Glyphs) {
		Memory::Free(Glyphs);
		Glyphs = nullptr;
	}
	if (GlyphBuffer) {
		Memory::Free(GlyphBuffer);
		GlyphBuffer = nullptr;
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
