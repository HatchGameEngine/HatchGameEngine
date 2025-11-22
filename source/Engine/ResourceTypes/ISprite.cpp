#include <Engine/ResourceTypes/ISprite.h>

#include <Engine/Application.h>
#include <Engine/Error.h>
#include <Engine/Graphics.h>

#include <Engine/ResourceTypes/Image.h>
#include <Engine/ResourceTypes/ResourceManager.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>

#include <Engine/IO/FileStream.h>
#include <Engine/IO/ResourceStream.h>

#include <Engine/Utilities/StringUtils.h>

#define RSDK_SPRITE_MAGIC 0x00525053

ISprite::ISprite() {
	Spritesheets.clear();
	Spritesheets.shrink_to_fit();
	SpritesheetFilenames.clear();
	SpritesheetFilenames.shrink_to_fit();
	LoadFailed = true;
	Filename = nullptr;
}
ISprite::ISprite(const char* filename) {
	Spritesheets.clear();
	Spritesheets.shrink_to_fit();
	SpritesheetFilenames.clear();
	SpritesheetFilenames.shrink_to_fit();
	Filename = StringUtils::NormalizePath(filename);
	LoadFailed = !LoadAnimation(Filename);
}

size_t ISprite::FindOrAddSpriteSheet(const char* sheetFilename) {
	std::string sheetPath = Path::Normalize(sheetFilename);

	for (size_t i = 0; i < SpritesheetFilenames.size(); i++) {
		if (SpritesheetFilenames[i] == sheetPath) {
			return i;
		}
	}

	AddSpriteSheet(sheetFilename);

	return Spritesheets.size() - 1;
}

Texture* ISprite::AddSpriteSheet(const char* sheetFilename) {
	Texture* texture = nullptr;

	char* filename = StringUtils::NormalizePath(sheetFilename);

	std::string sheetPath = std::string(filename);

	TextureReference* textureRef = Graphics::GetSpriteSheet(sheetPath);
	if (textureRef != nullptr) {
		SpritesheetFilenames.push_back(sheetPath);
		Spritesheets.push_back(textureRef->TexturePtr);
		Memory::Free(filename);
		return textureRef->TexturePtr;
	}

	texture = Image::LoadTextureFromResource(filename);

	Graphics::AddSpriteSheet(sheetPath, texture);
	Spritesheets.push_back(texture);
	SpritesheetFilenames.push_back(sheetPath);

	Memory::Free(filename);

	return texture;
}

void ISprite::ReserveAnimationCount(int count) {
	Animations.reserve(count);
}
void ISprite::AddAnimation(const char* name, int animationSpeed, int frameToLoop) {
	size_t strl = strlen(name);

	Animation an;
	an.Name = (char*)Memory::Malloc(strl + 1);
	strcpy(an.Name, name);
	an.Name[strl] = 0;
	an.AnimationSpeed = animationSpeed;
	an.FrameToLoop = frameToLoop;
	an.Flags = 0;
	Animations.push_back(an);
}
void ISprite::AddAnimation(const char* name, int animationSpeed, int frameToLoop, int frmAlloc) {
	AddAnimation(name, animationSpeed, frameToLoop);
	Animations.back().Frames.reserve(frmAlloc);
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
	AddFrame(Animations.size() - 1, duration, left, top, width, height, pivotX, pivotY, id);
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
	AnimFrame anfrm;
	anfrm.Advance = id;
	anfrm.Duration = duration;
	anfrm.X = left;
	anfrm.Y = top;
	anfrm.Width = width;
	anfrm.Height = height;
	anfrm.OffsetX = pivotX;
	anfrm.OffsetY = pivotY;
	anfrm.SheetNumber = sheetNumber;

	FrameCount++;

	Animations[animID].Frames.push_back(anfrm);
}
void ISprite::RemoveFrames(int animID) {
	FrameCount -= Animations[animID].Frames.size();
	Animations[animID].Frames.clear();
}

void ISprite::RefreshGraphicsID() {
	Graphics::MakeFrameBufferID(this);
}

void ISprite::ConvertToRGBA() {
	for (int a = 0; a < Spritesheets.size(); a++) {
		Graphics::ConvertTextureToRGBA(Spritesheets[a]);
	}
}
void ISprite::ConvertToPalette(unsigned paletteNumber) {
	for (int a = 0; a < Spritesheets.size(); a++) {
		Graphics::ConvertTextureToPalette(Spritesheets[a], paletteNumber);
	}
}

bool ISprite::IsFile(Stream* stream) {
	return stream->ReadUInt32() == RSDK_SPRITE_MAGIC;
}
bool ISprite::LoadAnimation(const char* filename) {
	char* str;
	int animationCount, previousAnimationCount;

	Stream* reader = ResourceStream::New(filename);
	if (!reader) {
		Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
		return false;
	}

#ifdef ISPRITE_DEBUG
	Log::Print(Log::LOG_VERBOSE, "\"%s\"", filename);
#endif

	/// =======================
	/// RSDKv5 Animation Format
	/// =======================

	// Check MAGIC
	if (!IsFile(reader)) {
		reader->Close();
		return false;
	}

	// Total frame count
	reader->ReadUInt32();

	// Get texture count
	unsigned spritesheetCount = reader->ReadByte();

	// Load textures
	for (int i = 0; i < spritesheetCount; i++) {
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

	// Get collision group count
	int hitboxCount = reader->ReadByte();

	// Read collision groups names
	std::vector<char*> hitboxNames;
	for (int i = 0; i < hitboxCount; i++) {
		hitboxNames.push_back(reader->ReadHeaderedString());
	}

	animationCount = reader->ReadUInt16();
	previousAnimationCount = (int)Animations.size();
	Animations.resize(previousAnimationCount + animationCount);

	// Load animations
	int frameID = 0;
	for (int a = 0; a < animationCount; a++) {
		Animation an;
		an.Name = reader->ReadHeaderedString();
		an.FrameCount = reader->ReadUInt16();
		an.FrameListOffset = frameID;
		an.AnimationSpeed = reader->ReadUInt16();
		an.FrameToLoop = reader->ReadByte();

		// 0: No rotation
		// 1: Full rotation
		// 2: Snaps to multiples of 45 degrees
		// 3: Snaps to multiples of 90 degrees
		// 4: Snaps to multiples of 180 degrees
		// 5: Static rotation using extra frames
		an.Flags = reader->ReadByte();

#ifdef ISPRITE_DEBUG
		Log::Print(Log::LOG_VERBOSE,
			"    \"%s\" (%d) (Flags: %02X, FtL: %d, Spd: %d, Frames: %d)",
			an.Name,
			a,
			an.Flags,
			an.FrameToLoop,
			an.AnimationSpeed,
			an.FrameCount);
#endif

		an.Frames.resize(an.FrameCount);

		for (int i = 0; i < an.FrameCount; i++) {
			AnimFrame anfrm;
			anfrm.SheetNumber = reader->ReadByte();
			frameID++;

			if (anfrm.SheetNumber >= Spritesheets.size()) {
				Log::Print(Log::LOG_ERROR,
					"Sheet number %d outside of range of sheet count %d! (Animation %d, Frame %d)",
					anfrm.SheetNumber,
					Spritesheets.size(),
					a,
					i);
			}

			anfrm.Duration = reader->ReadInt16();
			anfrm.Advance = reader->ReadUInt16();
			anfrm.X = reader->ReadUInt16();
			anfrm.Y = reader->ReadUInt16();
			anfrm.Width = reader->ReadUInt16();
			anfrm.Height = reader->ReadUInt16();
			anfrm.OffsetX = reader->ReadInt16();
			anfrm.OffsetY = reader->ReadInt16();

			for (int h = 0; h < hitboxCount; h++) {
				CollisionBox box;
				box.Name = std::string(hitboxNames[h]);
				box.Left = reader->ReadInt16();
				box.Top = reader->ReadInt16();
				box.Right = reader->ReadInt16();
				box.Bottom = reader->ReadInt16();
				anfrm.Boxes.push_back(box);
			}

#ifdef ISPRITE_DEBUG
			Log::Print(Log::LOG_VERBOSE,
				"       (X: %d, Y: %d, W: %d, H: %d, OffX: %d, OffY: %d)",
				anfrm.X,
				anfrm.Y,
				anfrm.Width,
				anfrm.Height,
				anfrm.OffsetX,
				anfrm.OffsetY);
#endif
			an.Frames[i] = anfrm;
		}
		Animations[previousAnimationCount + a] = an;
	}
	FrameCount = frameID;

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
	for (Uint32 a = 0; a < Animations.size(); a++) {
		if (Animations[a].Name[0] == animname[0] && !strcmp(Animations[a].Name, animname)) {
			return a;
		}
	}

	return -1;
}
void ISprite::LinkAnimation(vector<Animation> ani) {
	Animations = ani;
}
bool ISprite::SaveAnimation(const char* filename) {
	Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
		return false;
	}

	/// =======================
	/// RSDKv5 Animation Format
	/// =======================

	// Check MAGIC
	stream->WriteUInt32(RSDK_SPRITE_MAGIC);

	// Total frame count
	Uint32 totalFrameCount = 0;
	for (size_t a = 0; a < Animations.size(); a++) {
		totalFrameCount += Animations[a].Frames.size();
	}
	stream->WriteUInt32(totalFrameCount);

	// Get texture count
	stream->WriteByte(this->Spritesheets.size());

	// Load textures
	for (int i = 0; i < this->Spritesheets.size(); i++) {
		stream->WriteHeaderedString(SpritesheetFilenames[i].c_str());
	}

	// Get collision group count
	// In Hatch, the hitboxes are stored per frame. In RSDK's sprite format, the list of hitboxes
	// is shared for all frames. So we look through all sprite frames and build a list of unique
	// hitbox names. Frames hitboxes with a matching entry in boxNames are saved in that order.
	std::vector<std::string> boxNames;

	for (size_t a = 0; a < Animations.size(); a++) {
		Animation an = Animations[a];

		for (size_t i = 0; i < an.Frames.size(); i++) {
			AnimFrame anfrm = an.Frames[i];

			for (size_t h = 0; h < anfrm.Boxes.size(); h++) {
				CollisionBox box = anfrm.Boxes[h];
				auto it = std::find(boxNames.begin(), boxNames.end(), box.Name);
				if (it == boxNames.end()) {
					boxNames.push_back(box.Name);
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

	// Get animation count
	stream->WriteUInt16(Animations.size());

	// Write animations
	for (size_t a = 0; a < Animations.size(); a++) {
		Animation an = Animations[a];
		stream->WriteHeaderedString(an.Name);
		stream->WriteUInt16(an.Frames.size());
		stream->WriteUInt16(an.AnimationSpeed);
		stream->WriteByte(an.FrameToLoop);

		// 0: No rotation
		// 1: Full rotation
		// 2: Snaps to multiples of 45 degrees
		// 3: Snaps to multiples of 90 degrees
		// 4: Snaps to multiples of 180 degrees
		// 5: Static rotation using extra frames
		stream->WriteByte(an.Flags);

		for (size_t i = 0; i < an.Frames.size(); i++) {
			AnimFrame anfrm = an.Frames[i];
			stream->WriteByte(anfrm.SheetNumber);
			stream->WriteUInt16(anfrm.Duration);
			stream->WriteUInt16(anfrm.Advance);
			stream->WriteUInt16(anfrm.X);
			stream->WriteUInt16(anfrm.Y);
			stream->WriteUInt16(anfrm.Width);
			stream->WriteUInt16(anfrm.Height);
			stream->WriteInt16(anfrm.OffsetX);
			stream->WriteInt16(anfrm.OffsetY);

			for (size_t b = 0; b < totalBoxes; b++) {
				size_t h = 0;

				for (; h < anfrm.Boxes.size(); h++) {
					CollisionBox box = anfrm.Boxes[h];
					if (boxNames[b] == box.Name) {
						stream->WriteUInt16(box.Left);
						stream->WriteUInt16(box.Top);
						stream->WriteUInt16(box.Right);
						stream->WriteUInt16(box.Bottom);
						break;
					}
				}

				if (h == anfrm.Boxes.size()) {
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
	for (size_t a = 0; a < Animations.size(); a++) {
		if (Animations[a].Name) {
			Memory::Free(Animations[a].Name);
			Animations[a].Name = NULL;
		}

		RemoveFrames(a);

		Animations[a].Frames.clear();
		Animations[a].Frames.shrink_to_fit();
	}

	Animations.clear();
	Animations.shrink_to_fit();

	for (size_t i = 0; i < Spritesheets.size(); i++) {
		std::string sheetFilename = SpritesheetFilenames[i];
		if (sheetFilename.size() > 0) {
			Graphics::DisposeSpriteSheet(sheetFilename);
		}
	}

	Spritesheets.clear();
	Spritesheets.shrink_to_fit();

	SpritesheetFilenames.clear();
	SpritesheetFilenames.shrink_to_fit();

	Graphics::DeleteFrameBufferID(this);

	Memory::Free(Filename);
	Filename = nullptr;
}

ISprite::~ISprite() {
	Dispose();
}
