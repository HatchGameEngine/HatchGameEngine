#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/FontFace.h>

#ifdef USING_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H

FT_Library ftLib;
#endif
bool ftInitialized = false;

struct FT_GlyphBox {
	int ID;
	int X;
	int Y;
	int Width;
	int Height;
};
struct FT_Package {
	int Width;
	int Height;
};
struct FT_PackageNode {
	FT_GlyphBox* Box = NULL;
	bool Used = false;
	FT_PackageNode* Right = NULL;
	FT_PackageNode* Bottom = NULL;
};

FT_PackageNode* FindNode(FT_PackageNode* root, int width, int height) {
	if (root->Used) {
		FT_PackageNode* space;

		space = FindNode(root->Right, width, height);
		if (space) {
			return space;
		}

		space = FindNode(root->Bottom, width, height);
		if (space) {
			return space;
		}
	}
	else if (width <= root->Box->Width && height <= root->Box->Height) {
		return root;
	}
	return NULL;
}
FT_PackageNode* InsertNode(FT_PackageNode* parent, FT_GlyphBox* box, int width, int height) {
	parent->Used = true;

	// 'parent' is box we're inserting into
	parent->Right = new FT_PackageNode;
	parent->Right->Box = new FT_GlyphBox;
	parent->Right->Box->ID = -1;
	parent->Right->Box->X = parent->Box->X + width;
	parent->Right->Box->Y = parent->Box->Y;
	parent->Right->Box->Width = parent->Box->Width - width;
	parent->Right->Box->Height = height;

	parent->Bottom = new FT_PackageNode;
	parent->Bottom->Box = new FT_GlyphBox;
	parent->Bottom->Box->ID = -1;
	parent->Bottom->Box->X = parent->Box->X;
	parent->Bottom->Box->Y = parent->Box->Y + height;
	parent->Bottom->Box->Width = parent->Box->Width;
	parent->Bottom->Box->Height = parent->Box->Height - height;

	box->X = parent->Box->X;
	box->Y = parent->Box->Y;
	parent->Box = box;

	return parent;
}
FT_Package* PackBoxes(FT_GlyphBox* boxes, int boxCount, int defWidth, int defHeight) {
	FT_Package* package = new FT_Package;
	package->Width = defWidth;
	package->Height = defHeight;

	FT_PackageNode* root;
	root = new FT_PackageNode;
	root->Box = new FT_GlyphBox;
	root->Box->X = 0;
	root->Box->Y = 0;
	root->Box->Width = package->Width;
	root->Box->Height = package->Height;

	int padding = 1;
	int maxSz = 4096 << 4;

	bool done = true;
	bool increaseSide = false;
	while (true) {
		for (int i = 0; i < boxCount; i++) {
			if (!boxes[i].Width || !boxes[i].Height) {
				continue;
			}

			FT_PackageNode* node;
			// If we can find a place for this box
			if ((node = FindNode(root, boxes[i].Width, boxes[i].Height)) != NULL) {
				InsertNode(node,
					&boxes[i],
					boxes[i].Width + padding,
					boxes[i].Height + padding);
			}
			// If we can't
			else {
				// printf("package (%d x %d) not big
				// enough\n", package->Width,
				// package->Height);

				if (increaseSide) {
					package->Width <<= 1;
				}
				else {
					package->Height <<= 1;
				}
				increaseSide = !increaseSide;

				// printf("package increased to (%d x
				// %d)\n", package->Width,
				// package->Height);

				if (package->Width == 0 || package->Height == 0 ||
					(package->Width >= maxSz && package->Height >= maxSz)) {
					printf("package too big: %d x %d\n",
						package->Width,
						package->Height);
					return NULL;
				}

				done = false;

				root = new FT_PackageNode;
				root->Box = new FT_GlyphBox;
				root->Box->X = 0;
				root->Box->Y = 0;
				root->Box->Width = package->Width;
				root->Box->Height = package->Height;
				break;
			}
		}
		if (done) {
			break;
		}
		done = true;
	}

	return package;
}

ISprite* FontFace::SpriteFromFont(Stream* stream, int pixelSize, const char* filename) {
#ifdef USING_FREETYPE

	if (!ftInitialized) {
		if (FT_Init_FreeType(&ftLib)) {
			Log::Print(Log::LOG_ERROR, "FREETYPE: Could not init FreeType Library.");
			stream->Close();
			return NULL;
		}
		ftInitialized = true;
	}

	void* fontFileMemory = Memory::Malloc(stream->Length());
	size_t fontFileLength = stream->Length();
	stream->ReadBytes(fontFileMemory, fontFileLength);

	FT_Face face;
	if (FT_New_Memory_Face(ftLib, (Uint8*)fontFileMemory, fontFileLength, 0, &face)) {
		Log::Print(Log::LOG_ERROR, "FREETYPE: Failed to load font.");
		Memory::Free(fontFileMemory);
		return NULL;
	}
	FT_Set_Pixel_Sizes(face, 0, pixelSize);

	FT_GlyphBox boxes[0x100];
	memset(boxes, 0, sizeof(boxes));

	for (Uint32 c = ' '; c < 0x100; c++) {
		boxes[c].ID = c;
		if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
			continue;
		}

		boxes[c].Width = face->glyph->bitmap.width + 1;
		boxes[c].Height = face->glyph->bitmap.rows + 1;
	}

	FT_Package* package = PackBoxes(boxes, 0x100, 256, 256);

	// Allocate pixel memory
	ISprite* sprite = new ISprite();
	Uint32* pixelData =
		(Uint32*)Memory::Malloc(package->Width * package->Height * sizeof(Uint32));
	Uint32 pixelStride = package->Width * sizeof(Uint32);
	Memory::Memset4(pixelData, 0x00FFFFFF, package->Width * package->Height);

	int offsetSlightX = 0;
	int offsetBaseline = 0;
	if (!FT_Load_Char(face, 'E', FT_LOAD_RENDER)) {
		offsetSlightX = face->glyph->bitmap_left;
		offsetBaseline = face->glyph->bitmap_top;
	}

	for (Uint32 c = ' '; c < 0x100; c++) {
		if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
			continue;
		}

		Uint8* buf = face->glyph->bitmap.buffer;
		char* pixel_start =
			(char*)(pixelData + boxes[c].X + boxes[c].Y * package->Width) + 3;
		for (Uint32 py = 0; py < face->glyph->bitmap.rows; py++) {
			for (char* pixel_a = pixel_start;
				pixel_a < pixel_start + face->glyph->bitmap.width * 4;
				pixel_a += 4) {
				// Set alpha
				*pixel_a = *buf++;
			}
			pixel_start += pixelStride;
		}
	}

	Texture* spriteSheet = Graphics::CreateTextureFromPixels(
		package->Width, package->Height, pixelData, package->Width * sizeof(Uint32));
	if (spriteSheet) {
		sprite->Spritesheets.push_back(spriteSheet);
		sprite->SpritesheetFilenames.push_back(std::string(filename));
	}

	// Add preliminary chars
	sprite->AddAnimation("Font", offsetBaseline & 0xFFFF, pixelSize, 0x100);
	for (Uint32 c = 0; c < ' '; c++) {
		sprite->AddFrame(0, 0, 0, 1, 1, 0, 0, 0);
	}

	for (Uint32 c = ' '; c < 0x100; c++) {
		if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
			Log::Print(Log::LOG_ERROR, "FREETYTPE: Failed to load Glyph %X (%c)", c, c);
			sprite->AddFrame(0, 0, 0, 1, 1, 0, 0, 0);
			continue;
		}

		sprite->AddFrame(0,
			boxes[c].X,
			boxes[c].Y,
			face->glyph->bitmap.width,
			face->glyph->bitmap.rows,
			face->glyph->bitmap_left + offsetSlightX,
			-face->glyph->bitmap_top + offsetBaseline,
			0);
		sprite->Animations.back().Frames.back().Advance = face->glyph->advance.x >> 6;
	}

	bool exportFonts = false;
	Application::Settings->GetBool("dev", "exportFonts", &exportFonts);
	if (exportFonts) {
		const char* filenameJustName = filename + strlen(filename) - 1;
		for (; filenameJustName > filename; filenameJustName--) {
			if (*filenameJustName == '/') {
				filenameJustName++;
				break;
			}
		}

		char testFilename[MAX_RESOURCE_PATH_LENGTH];
		snprintf(testFilename,
			sizeof testFilename,
			"Fonts/%s_%d.bmp",
			filenameJustName,
			pixelSize);
		sprite->SpritesheetFilenames.push_back(std::string(testFilename));

		SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixelData,
			package->Width,
			package->Height,
			32,
			package->Width * 4,
			0x00FF0000,
			0x0000FF00,
			0x000000FF,
			0xFF000000);
		SDL_SaveBMP(surface, testFilename);
		SDL_FreeSurface(surface);

		snprintf(testFilename,
			sizeof testFilename,
			"Fonts/%s_%d.bin",
			filenameJustName,
			pixelSize);
		sprite->SaveAnimation(testFilename);
	}

	sprite->RefreshGraphicsID();

	Memory::Free(fontFileMemory);
	Memory::Free(pixelData);
	FT_Done_Face(face);
	// FT_Done_FreeType(ftLib);

	return sprite;

#endif
	return NULL;
}
