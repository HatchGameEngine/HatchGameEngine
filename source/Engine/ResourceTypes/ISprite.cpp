#include <Engine/ResourceTypes/ISprite.h>

#include <Engine/Application.h>
#include <Engine/Graphics.h>

#include <Engine/ResourceTypes/ImageFormats/GIF.h>
#include <Engine/ResourceTypes/ImageFormats/JPEG.h>
#include <Engine/ResourceTypes/ImageFormats/PNG.h>

#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>

#include <Engine/IO/FileStream.h>
#include <Engine/IO/ResourceStream.h>

#include <Engine/Utilities/StringUtils.h>

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
	Texture* texture = NULL;
	Uint32* data = NULL;
	Uint32 width = 0;
	Uint32 height = 0;
	Uint32* paletteColors = NULL;
	unsigned numPaletteColors = 0;

	char* filename = StringUtils::NormalizePath(sheetFilename);

	std::string sheetPath = std::string(filename);

	TextureReference* textureRef = Graphics::GetSpriteSheet(sheetPath);
	if (textureRef != nullptr) {
		SpritesheetFilenames.push_back(sheetPath);
		Spritesheets.push_back(textureRef->TexturePtr);
		Memory::Free(filename);
		return textureRef->TexturePtr;
	}

	float loadDelta = 0.0f;
	if (StringUtils::StrCaseStr(filename, ".png")) {
		Clock::Start();
		PNG* png = PNG::Load(filename);
		loadDelta = Clock::End();

		if (png && png->Data) {
			Log::Print(Log::LOG_VERBOSE,
				"PNG load took %.3f ms (%s)",
				loadDelta,
				filename);
			width = png->Width;
			height = png->Height;

			data = png->Data;
			Memory::Track(data, "Texture::Data");

			if (png->Paletted) {
				paletteColors = png->GetPalette();
				numPaletteColors = png->NumPaletteColors;
			}

			delete png;
		}
		else {
			Log::Print(Log::LOG_ERROR, "PNG could not be loaded!");
			Memory::Free(filename);
			return NULL;
		}
	}
	else if (StringUtils::StrCaseStr(filename, ".jpg") ||
		StringUtils::StrCaseStr(filename, ".jpeg")) {
		Clock::Start();
		JPEG* jpeg = JPEG::Load(filename);
		loadDelta = Clock::End();

		if (jpeg && jpeg->Data) {
			Log::Print(Log::LOG_VERBOSE,
				"JPEG load took %.3f ms (%s)",
				loadDelta,
				filename);
			width = jpeg->Width;
			height = jpeg->Height;

			data = jpeg->Data;
			Memory::Track(data, "Texture::Data");

			delete jpeg;
		}
		else {
			Log::Print(Log::LOG_ERROR, "JPEG could not be loaded!");
			Memory::Free(filename);
			return NULL;
		}
	}
	else if (StringUtils::StrCaseStr(filename, ".gif")) {
		Clock::Start();
		GIF* gif = GIF::Load(filename);
		loadDelta = Clock::End();

		if (gif && gif->Data) {
			Log::Print(Log::LOG_VERBOSE,
				"GIF load took %.3f ms (%s)",
				loadDelta,
				filename);
			width = gif->Width;
			height = gif->Height;

			data = gif->Data;
			Memory::Track(data, "Texture::Data");

			if (gif->Paletted) {
				paletteColors = gif->GetPalette();
				numPaletteColors = gif->NumPaletteColors;
			}

			delete gif;
		}
		else {
			Log::Print(Log::LOG_ERROR, "GIF could not be loaded!");
			Memory::Free(filename);
			return NULL;
		}
	}
	else {
		Log::Print(Log::LOG_ERROR, "Unsupported image format for sprite!");
		Memory::Free(filename);
		return texture;
	}

	bool forceSoftwareTextures = false;
	Application::Settings->GetBool("display", "forceSoftwareTextures", &forceSoftwareTextures);
	if (forceSoftwareTextures) {
		Graphics::NoInternalTextures = true;
	}

	texture = Graphics::CreateTextureFromPixels(width, height, data, width * sizeof(Uint32));

	if (texture == NULL) {
		Log::Print(Log::LOG_ERROR, "Couldn't create sprite sheet texture!");
		abort();
	}

	Graphics::SetTexturePalette(texture, paletteColors, numPaletteColors);

	Graphics::NoInternalTextures = false;

	Memory::Free(data);

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
	anfrm.BoxCount = 0;

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
	if (reader->ReadUInt32() != 0x00525053) {
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

		bool shouldConcatSpritesPath = true;
		if (StringUtils::StartsWith(sheetName.c_str(), "Sprites/")) {
			// don't need to concat "Sprites/" if the path
			// already begins with that
			shouldConcatSpritesPath = false;
		}

		if (shouldConcatSpritesPath) {
			std::string altered = Path::Concat(std::string("Sprites"), sheetName);

			AddSpriteSheet(altered.c_str());
		}
		else {
			AddSpriteSheet(sheetName.c_str());
		}
	}

	// Get collision group count
	this->CollisionBoxCount = reader->ReadByte();

	// Load collision groups
	for (int i = 0; i < this->CollisionBoxCount; i++) {
		Memory::Free(reader->ReadHeaderedString());
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

			anfrm.BoxCount = this->CollisionBoxCount;
			if (anfrm.BoxCount) {
				anfrm.Boxes = (CollisionBox*)Memory::Malloc(
					anfrm.BoxCount * sizeof(CollisionBox));
				for (int h = 0; h < anfrm.BoxCount; h++) {
					anfrm.Boxes[h].Left = reader->ReadInt16();
					anfrm.Boxes[h].Top = reader->ReadInt16();
					anfrm.Boxes[h].Right = reader->ReadInt16();
					anfrm.Boxes[h].Bottom = reader->ReadInt16();
				}
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
	stream->WriteUInt32(0x00525053);

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
	stream->WriteByte(this->CollisionBoxCount);

	// Write collision groups
	for (int i = 0; i < this->CollisionBoxCount; i++) {
		stream->WriteHeaderedString("Dummy");
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

			for (int h = 0; h < anfrm.BoxCount; h++) {
				stream->WriteUInt16(anfrm.Boxes[h].Left);
				stream->WriteUInt16(anfrm.Boxes[h].Top);
				stream->WriteUInt16(anfrm.Boxes[h].Right);
				stream->WriteUInt16(anfrm.Boxes[h].Bottom);
			}
		}
	}
	stream->Close();

	return true;
}

void ISprite::Dispose() {
	for (size_t a = 0; a < Animations.size(); a++) {
		for (size_t i = 0; i < Animations[a].Frames.size(); i++) {
			AnimFrame* anfrm = &Animations[a].Frames[i];
			if (anfrm->BoxCount) {
				Memory::Free(anfrm->Boxes);
				anfrm->Boxes = NULL;
			}
		}
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

	for (int a = 0; a < Spritesheets.size(); a++) {
		Graphics::DisposeSpriteSheet(SpritesheetFilenames[a]);
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
