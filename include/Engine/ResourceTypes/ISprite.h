#ifndef ENGINE_RESOURCETYPES_ISPRITE_H
#define ENGINE_RESOURCETYPES_ISPRITE_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Sprites/Animation.h>

class ISprite {
public:
	char* Filename = nullptr;
	bool LoadFailed = true;
	Texture** Spritesheets;
	char** SpritesheetFilenames;
	Animation* Animations = nullptr;
	AnimFrame* Frames = nullptr;
	size_t AnimationCount = 0;
	size_t AnimationsCapacity = 1;
	size_t FrameCount = 0;
	size_t FramesCapacity = 1;
	size_t SpritesheetCount = 0;
	size_t SpritesheetsCapacity = 1;
	int ID = 0;

	ISprite();
	ISprite(const char* filename);
	void AddSpriteSheet(Texture* texture, char* filename);
	Texture* AddSpriteSheet(const char* sheetFilename);
	size_t FindOrAddSpriteSheet(const char* sheetFilename);
	void ReserveAnimationCount(int count);
	void AddAnimation(const char* name, int animationSpeed, int frameToLoop);
	void AddAnimation(const char* name, int animationSpeed, int frameToLoop, int frmAlloc);
	void
	AddFrame(int duration, int left, int top, int width, int height, int pivotX, int pivotY);
	void AddFrame(int duration,
		int left,
		int top,
		int width,
		int height,
		int pivotX,
		int pivotY,
		int id);
	void AddFrame(int animID,
		int duration,
		int left,
		int top,
		int width,
		int height,
		int pivotX,
		int pivotY,
		int id);
	void AddFrame(int animID,
		int duration,
		int left,
		int top,
		int width,
		int height,
		int pivotX,
		int pivotY,
		int id,
		int sheetNumber);
	void RemoveFrames(int animID);
	void RefreshGraphicsID();
	void ConvertToNonIndexed(Uint32* palColors, unsigned numPaletteColors);
	void ConvertToIndexed(Uint32* palColors, unsigned numPaletteColors);
	static bool IsFile(Stream* stream);
	bool LoadAnimation(const char* filename);
	int FindAnimation(const char* animname);
	bool SaveAnimation(const char* filename);
	void Dispose();
	~ISprite();
};

#endif /* ENGINE_RESOURCETYPES_ISPRITE_H */
