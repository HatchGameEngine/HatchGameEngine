#include <Engine/ResourceTypes/ISprite.h>

#include <Engine/Application.h>
#include <Engine/Error.h>
#include <Engine/Graphics.h>

#include <Engine/ResourceTypes/Image.h>
#include <Engine/ResourceTypes/ResourceManager.h>

#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>

#include <Engine/IO/FileStream.h>
#include <Engine/IO/ResourceStream.h>

#include <Engine/Utilities/StringUtils.h>

#define RSDK_SPRITE_MAGIC 0x00525053

ISprite::ISprite() {
	LoadFailed = true;
	Filename = nullptr;
}
ISprite::ISprite(const char* filename) {
	Filename = StringUtils::NormalizePath(filename);
	LoadFailed = !LoadAnimation(Filename);
}

size_t ISprite::FindOrAddSpriteSheet(const char* sheetFilename) {
	std::string sheetPath = Path::Normalize(sheetFilename);

	for (size_t i = 0; i < SpritesheetCount; i++) {
		if (strcmp(SpritesheetFilenames[i], sheetPath.c_str()) == 0) {
			return i;
		}
	}

	AddSpriteSheet(sheetFilename);

	return SpritesheetCount - 1;
}

void ISprite::AddSpriteSheet(Texture* texture, char* filename) {
	SpritesheetCount++;
	if (!Spritesheets || SpritesheetCount > SpritesheetsCapacity) {
		SpritesheetsCapacity *= 2;
		Spritesheets = (Texture**)Memory::Realloc(Spritesheets, SpritesheetsCapacity * sizeof(Texture*));
		SpritesheetFilenames = (char**)Memory::Realloc(SpritesheetFilenames, SpritesheetsCapacity * sizeof(char*));
	}

	Spritesheets[SpritesheetCount - 1] = texture;
	SpritesheetFilenames[SpritesheetCount - 1] = filename;
}

Texture* ISprite::AddSpriteSheet(const char* sheetFilename) {
	Texture* texture = nullptr;

	char* filename = StringUtils::NormalizePath(sheetFilename);

	std::string sheetPath = std::string(filename);

	TextureReference* textureRef = Graphics::GetSpriteSheet(sheetPath);
	if (textureRef != nullptr) {
		AddSpriteSheet(textureRef->TexturePtr, filename);

		return textureRef->TexturePtr;
	}

	texture = Image::LoadTextureFromResource(filename);

	Graphics::AddSpriteSheet(sheetPath, texture);

	AddSpriteSheet(texture, filename);

	return texture;
}

void ISprite::ReserveAnimationCount(int count) {
	int capacity = Math::CeilPOT(count);
	if (capacity > AnimationsCapacity) {
		size_t previousAnimationsCapacity = AnimationsCapacity;
		AnimationsCapacity = capacity;
		Animations = (Animation*)Memory::Realloc(Animations, AnimationsCapacity * sizeof(Animation));
		memset(&Animations[previousAnimationsCapacity], 0x00, (AnimationsCapacity - previousAnimationsCapacity) * sizeof(Animation));
	}
}
void ISprite::AddAnimation(const char* name, int animationSpeed, int frameToLoop) {
	AnimationCount++;
	if (AnimationCount >= AnimationsCapacity) {
		AnimationsCapacity *= 2;
		Animations = (Animation*)Memory::Realloc(Animations, AnimationsCapacity * sizeof(Animation));
	}

	Animation* an = &Animations[AnimationCount - 1];
	an->Name = StringUtils::Duplicate(name);
	an->AnimationSpeed = animationSpeed;
	an->FrameToLoop = frameToLoop;
	an->Flags = 0;
	an->FrameListOffset = FrameCount;
	an->Frames = &Frames[an->FrameListOffset];
	an->FrameCount = 0;
}
void ISprite::AddAnimation(const char* name, int animationSpeed, int frameToLoop, int frmAlloc) {
	if (frmAlloc > 0) {
		size_t previousFramesCapacity = FramesCapacity;
		FramesCapacity = Math::CeilPOT(FramesCapacity + frmAlloc);
		Frames = (AnimFrame*)Memory::Realloc(Frames, FramesCapacity * sizeof(AnimFrame));
		memset(&Frames[previousFramesCapacity], 0x00, (FramesCapacity - previousFramesCapacity) * sizeof(AnimFrame));

		for (size_t a = 0; a < AnimationCount; a++) {
			Animations[a].Frames = &Frames[Animations[a].FrameListOffset];
		}
	}

	AddAnimation(name, animationSpeed, frameToLoop);
}
void ISprite::AddFrame(int duration,
	int left,
	int top,
	int width,
	int height,
	int pivotX,
	int pivotY) {
	ISprite::AddFrame(duration, left, top, width, height, pivotX, pivotY, 0);
}
void ISprite::AddFrame(int duration,
	int left,
	int top,
	int width,
	int height,
	int pivotX,
	int pivotY,
	int id) {
	AddFrame(AnimationCount - 1, duration, left, top, width, height, pivotX, pivotY, id);
}
void ISprite::AddFrame(int animID,
	int duration,
	int left,
	int top,
	int width,
	int height,
	int pivotX,
	int pivotY,
	int id) {
	AddFrame(animID, duration, left, top, width, height, pivotX, pivotY, id, 0);
}
void ISprite::AddFrame(int animID,
	int duration,
	int left,
	int top,
	int width,
	int height,
	int pivotX,
	int pivotY,
	int id,
	int sheetNumber) {
	// TODO
	if (animID != AnimationCount - 1) {
		return;
	}

	FrameCount++;
	if (FrameCount >= FramesCapacity) {
		size_t previousFramesCapacity = FramesCapacity;
		FramesCapacity *= 2;
		Frames = (AnimFrame*)Memory::Realloc(Frames, FramesCapacity * sizeof(AnimFrame));
		memset(&Frames[previousFramesCapacity], 0x00, (FramesCapacity - previousFramesCapacity) * sizeof(AnimFrame));

		for (size_t a = 0; a < AnimationCount; a++) {
			Animations[a].Frames = &Frames[Animations[a].FrameListOffset];
		}
	}

	AnimFrame* anfrm = &Frames[FrameCount - 1];
	anfrm->Advance = id;
	anfrm->Duration = duration;
	anfrm->X = left;
	anfrm->Y = top;
	anfrm->Width = width;
	anfrm->Height = height;
	anfrm->OffsetX = pivotX;
	anfrm->OffsetY = pivotY;
	anfrm->SheetNumber = sheetNumber;
	anfrm->BoxCount = 0;
	anfrm->Boxes = nullptr;

	Animations[animID].FrameCount++;
}
void ISprite::RemoveFrames(int animID) {
	// TODO
	FrameCount -= Animations[animID].FrameCount;
}

void ISprite::RefreshGraphicsID() {
	Graphics::MakeFrameBufferID(this);
}

void ISprite::ConvertToNonIndexed(Uint32* palColors, unsigned numPaletteColors) {
	for (int a = 0; a < SpritesheetCount; a++) {
		Uint32* palette = palColors;
		if (!palette) {
			if (Spritesheets[a]->PaletteColors) {
				palette = Spritesheets[a]->PaletteColors;
				numPaletteColors = Spritesheets[a]->NumPaletteColors;
			}
			else {
				palette = Graphics::PaletteColors[0];
				numPaletteColors = 256;
			}
		}

		Graphics::ConvertTextureToFormat(
			Spritesheets[a], Graphics::TextureFormat, palette, numPaletteColors, 0);
	}
}
void ISprite::ConvertToIndexed(Uint32* palColors, unsigned numPaletteColors) {
	int transparent = 0;
	if (palColors != nullptr) {
		transparent = Graphics::GetPaletteTransparentColor(palColors, numPaletteColors);
		if (transparent == -1) {
			transparent = 0;
		}
	}

	for (int a = 0; a < SpritesheetCount; a++) {
		Graphics::ConvertTextureToFormat(Spritesheets[a],
			TextureFormat_INDEXED,
			palColors,
			numPaletteColors,
			transparent);
	}
}

bool ISprite::IsFile(Stream* stream) {
	return stream->ReadUInt32() == RSDK_SPRITE_MAGIC;
}
bool ISprite::LoadAnimation(const char* filename) {
	char* str;
	size_t totalFrameCount, previousFrameCount;
	size_t animationCount, previousAnimationCount;

	Stream* reader = ResourceStream::New(filename);
	if (!reader) {
		Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
		return false;
	}

#ifdef ISPRITE_DEBUG
	Log::Print(Log::LOG_VERBOSE, "\"%s\"", filename);
#endif

	// Check MAGIC
	if (!IsFile(reader)) {
		reader->Close();
		return false;
	}

	// Read total frame count
	totalFrameCount = reader->ReadUInt32();

	// Get texture count
	unsigned spritesheetCount = reader->ReadByte();
	SpritesheetsCapacity = Math::CeilPOT(SpritesheetsCapacity + spritesheetCount);
	Spritesheets = (Texture**)Memory::Malloc(sizeof(Texture*) * SpritesheetsCapacity);
	SpritesheetFilenames = (char**)Memory::Malloc(sizeof(char*) * SpritesheetsCapacity);

	// Load textures
	for (size_t i = 0; i < spritesheetCount; i++) {
		char fullPath[MAX_RESOURCE_PATH_LENGTH];

		str = reader->ReadHeaderedString();

		// Spritesheet path is relative to where the animation
		// file is
		if (StringUtils::StartsWith(str, "./")) {
			char* parentPath = StringUtils::GetPath(filename);
			if (parentPath) {
				snprintf(fullPath, sizeof fullPath, "%s/%s", parentPath, str + 2);
				Memory::Free(parentPath);
			}
			else {
				snprintf(fullPath, sizeof fullPath, "%s", str + 2);
			}
		}
		else {
			StringUtils::Copy(fullPath, str, sizeof fullPath);
		}

		Memory::Free(str);

		// Replace '\' with '/'
		StringUtils::ReplacePathSeparatorsInPlace(fullPath);

		std::string sheetName = std::string(fullPath);

#ifdef ISPRITE_DEBUG
		Log::Print(Log::LOG_VERBOSE, " - %s", sheetName.c_str());
#endif

		// If the resource doesn't exist, and the path doesn't begin with 'Sprites/' or 'sprites/'
		if (!ResourceManager::ResourceExists(sheetName.c_str()) &&
			!StringUtils::StartsWithCaseInsensitive(sheetName.c_str(), "Sprites/")) {
			std::string altered = Path::Normalize(Path::Concat("Sprites", sheetName));

			// Try with 'sprites/' if the above doesn't exist
			if (!ResourceManager::ResourceExists(altered.c_str())) {
				altered = Path::Normalize(Path::Concat("sprites", sheetName));
			}

			AddSpriteSheet(altered.c_str());
		}
		else {
			AddSpriteSheet(sheetName.c_str());
		}
	}

	Clock::Start();

	previousFrameCount = FrameCount;
	FrameCount += totalFrameCount;
	if (FrameCount && (Frames == nullptr || FrameCount > FramesCapacity)) {
		FramesCapacity = Math::CeilPOT(FrameCount);
		Frames = (AnimFrame*)Memory::Realloc(Frames, FramesCapacity * sizeof(AnimFrame));

		for (size_t a = 0; a < AnimationCount; a++) {
			Animations[a].Frames = &Frames[Animations[a].FrameListOffset];
		}
	}

	// Get collision group count
	int hitboxCount = reader->ReadByte();

	// Read collision groups names
	std::vector<char*> hitboxNames;
	hitboxNames.resize(hitboxCount);
	for (int i = 0; i < hitboxCount; i++) {
		hitboxNames[i] = reader->ReadHeaderedString();
	}

	previousAnimationCount = AnimationCount;
	animationCount = reader->ReadUInt16();
	AnimationCount += animationCount;
	if (Animations == nullptr || AnimationCount > AnimationsCapacity) {
		AnimationsCapacity = Math::CeilPOT(AnimationCount);
		Animations = (Animation*)Memory::Realloc(Animations, AnimationsCapacity * sizeof(Animation));
	}

	// Load animations
	size_t frameID = previousFrameCount;
	for (size_t a = 0; a < animationCount; a++) {
		Animation* an = &Animations[previousAnimationCount + a];
		an->Name = reader->ReadHeaderedString();
		an->FrameCount = reader->ReadUInt16();
		an->FrameListOffset = frameID;
		an->AnimationSpeed = reader->ReadUInt16();
		an->FrameToLoop = reader->ReadByte();

		// 0: No rotation
		// 1: Full rotation
		// 2: Snaps to multiples of 45 degrees
		// 3: Snaps to multiples of 90 degrees
		// 4: Snaps to multiples of 180 degrees
		// 5: Static rotation using extra frames
		an->Flags = reader->ReadByte();

#ifdef ISPRITE_DEBUG
		Log::Print(Log::LOG_VERBOSE,
			"    \"%s\" (%d) (Flags: %02X, FtL: %d, Spd: %d, Frames: %d)",
			an->Name,
			a,
			an->Flags,
			an->FrameToLoop,
			an->AnimationSpeed,
			an->FrameCount);
#endif

		frameID += an->FrameCount;

		// Safeguard in case totalFrameCount was zero
		if (frameID > FrameCount) {
			FrameCount = frameID;
			FramesCapacity = Math::CeilPOT(FrameCount);
			Frames = (AnimFrame*)Memory::Realloc(Frames, FramesCapacity * sizeof(AnimFrame));

			for (size_t lastAnimIndex = 0; lastAnimIndex < a; lastAnimIndex++) {
				Animations[lastAnimIndex].Frames = &Frames[Animations[lastAnimIndex].FrameListOffset];
			}
		}

		an->Frames = &Frames[an->FrameListOffset];

		for (size_t i = 0; i < an->FrameCount; i++) {
			AnimFrame* anfrm = &an->Frames[i];
			anfrm->SheetNumber = reader->ReadByte();

			if (anfrm->SheetNumber >= SpritesheetCount) {
				Log::Print(Log::LOG_ERROR,
					"Sheet number %d outside of range of sheet count %d! (Animation %d, Frame %d)",
					anfrm->SheetNumber,
					SpritesheetCount,
					a,
					i);
			}

			anfrm->Duration = reader->ReadInt16();
			anfrm->Advance = reader->ReadUInt16();
			anfrm->X = reader->ReadUInt16();
			anfrm->Y = reader->ReadUInt16();
			anfrm->Width = reader->ReadUInt16();
			anfrm->Height = reader->ReadUInt16();
			anfrm->OffsetX = reader->ReadInt16();
			anfrm->OffsetY = reader->ReadInt16();
			anfrm->BoxCount = hitboxCount;

			if (anfrm->BoxCount) {
				anfrm->Boxes = (CollisionBox*)Memory::Malloc(anfrm->BoxCount * sizeof(CollisionBox));

				for (size_t h = 0; h < anfrm->BoxCount; h++) {
					CollisionBox* box = &anfrm->Boxes[h];
					StringUtils::Copy(box->Name, hitboxNames[h], sizeof box->Name);
					box->Left = reader->ReadInt16();
					box->Top = reader->ReadInt16();
					box->Right = reader->ReadInt16();
					box->Bottom = reader->ReadInt16();
				}
			}
			else {
				anfrm->Boxes = nullptr;
			}

#ifdef ISPRITE_DEBUG
			Log::Print(Log::LOG_VERBOSE,
				"       (X: %d, Y: %d, W: %d, H: %d, OffX: %d, OffY: %d)",
				anfrm->X,
				anfrm->Y,
				anfrm->Width,
				anfrm->Height,
				anfrm->OffsetX,
				anfrm->OffsetY);
#endif
		}
	}
	FrameCount = frameID;

	Log::Print(Log::LOG_VERBOSE, "Sprite \"%s\" took %.3f ms to load", filename, Clock::End());

	// Possibly buffer the position in the renderer.
	Graphics::MakeFrameBufferID(this);

	// Free the read collision group names
	for (int i = 0; i < hitboxCount; i++) {
		Memory::Free(hitboxNames[i]);
	}

	reader->Close();

	return true;
}
int ISprite::FindAnimation(const char* animname) {
	for (Uint32 a = 0; a < AnimationCount; a++) {
		if (Animations[a].Name[0] == animname[0] && !strcmp(Animations[a].Name, animname)) {
			return a;
		}
	}

	return -1;
}
bool ISprite::SaveAnimation(const char* filename) {
	Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
		return false;
	}

	// Write MAGIC
	stream->WriteUInt32(RSDK_SPRITE_MAGIC);

	// Write total frame count
	stream->WriteUInt32(FrameCount);

	// Write texture count
	stream->WriteByte(SpritesheetCount);

	// Write textures
	for (int i = 0; i < SpritesheetCount; i++) {
		stream->WriteHeaderedString(SpritesheetFilenames[i]);
	}

	// Get collision group count
	// In Hatch, the hitboxes are stored per frame. In RSDK's sprite format, the list of hitboxes
	// is shared for all frames. So we look through all sprite frames and build a list of unique
	// hitbox names. Frames hitboxes with a matching entry in boxNames are saved in that order.
	std::vector<std::string> boxNames;

	for (size_t a = 0; a < AnimationCount; a++) {
		Animation* an = &Animations[a];

		for (size_t i = 0; i < an->FrameCount; i++) {
			AnimFrame* anfrm = &an->Frames[i];

			for (size_t h = 0; h < anfrm->BoxCount; h++) {
				CollisionBox* box = &anfrm->Boxes[h];
				auto it = std::find(boxNames.begin(), boxNames.end(), box->Name);
				if (it == boxNames.end()) {
					boxNames.push_back(box->Name);
				}
			}
		}
	}

	size_t totalBoxes = boxNames.size();
	if (totalBoxes > 0xFF) {
		totalBoxes = 0xFF;
	}
	stream->WriteByte(totalBoxes);

	// Write collision groups
	for (size_t i = 0; i < totalBoxes; i++) {
		stream->WriteHeaderedString(boxNames[i].c_str());
	}

	// Write animation count
	stream->WriteUInt16(AnimationCount);

	// Write animations
	for (size_t a = 0; a < AnimationCount; a++) {
		Animation* an = &Animations[a];
		stream->WriteHeaderedString(an->Name);
		stream->WriteUInt16(an->FrameCount);
		stream->WriteUInt16(an->AnimationSpeed);
		stream->WriteByte(an->FrameToLoop);

		// 0: No rotation
		// 1: Full rotation
		// 2: Snaps to multiples of 45 degrees
		// 3: Snaps to multiples of 90 degrees
		// 4: Snaps to multiples of 180 degrees
		// 5: Static rotation using extra frames
		stream->WriteByte(an->Flags);

		for (size_t i = 0; i < an->FrameCount; i++) {
			AnimFrame* anfrm = &an->Frames[i];
			stream->WriteByte(anfrm->SheetNumber);
			stream->WriteUInt16(anfrm->Duration);
			stream->WriteUInt16(anfrm->Advance);
			stream->WriteUInt16(anfrm->X);
			stream->WriteUInt16(anfrm->Y);
			stream->WriteUInt16(anfrm->Width);
			stream->WriteUInt16(anfrm->Height);
			stream->WriteInt16(anfrm->OffsetX);
			stream->WriteInt16(anfrm->OffsetY);

			for (size_t b = 0; b < totalBoxes; b++) {
				size_t h = 0;

				for (; h < anfrm->BoxCount; h++) {
					CollisionBox box = anfrm->Boxes[h];
					if (boxNames[b] == box.Name) {
						stream->WriteUInt16(box.Left);
						stream->WriteUInt16(box.Top);
						stream->WriteUInt16(box.Right);
						stream->WriteUInt16(box.Bottom);
						break;
					}
				}

				if (h == anfrm->BoxCount) {
					stream->WriteUInt16(0);
					stream->WriteUInt16(0);
					stream->WriteUInt16(0);
					stream->WriteUInt16(0);
				}
			}
		}
	}
	stream->Close();

	return true;
}

void ISprite::Dispose() {
	for (size_t f = 0; f < FrameCount; f++) {
		Memory::Free(Frames[f].Boxes);
	}
	FrameCount = 0;
	FramesCapacity = 0;
	Memory::Free(Frames);
	Frames = nullptr;

	for (size_t a = 0; a < AnimationCount; a++) {
		Memory::Free(Animations[a].Name);
	}
	AnimationCount = 0;
	AnimationsCapacity = 0;
	Memory::Free(Animations);
	Animations = nullptr;

	for (size_t i = 0; i < SpritesheetCount; i++) {
		char* sheetFilename = SpritesheetFilenames[i];
		if (sheetFilename) {
			Graphics::DisposeSpriteSheet(sheetFilename);
			Memory::Free(sheetFilename);
		}
	}
	SpritesheetCount = 0;
	SpritesheetsCapacity = 0;

	Memory::Free(Spritesheets);
	Memory::Free(SpritesheetFilenames);

	Graphics::DeleteFrameBufferID(this);

	Memory::Free(Filename);
	Filename = nullptr;
}

ISprite::~ISprite() {
	Dispose();
}
