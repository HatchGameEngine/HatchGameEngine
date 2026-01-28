#ifndef ENGINE_RESOURCETYPES_ISPRITE_H
#define ENGINE_RESOURCETYPES_ISPRITE_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/Asset.h>
#include <Engine/Sprites/Animation.h>

class ISprite : public Asset {
public:
	vector<Texture*> Spritesheets;
	vector<string> SpritesheetFilenames;
	vector<Animation> Animations;
	int ID = 0;
	int FrameCount = 0;

	ISprite();
	ISprite(const char* filename);
	Texture* AddSpriteSheet(const char* sheetFilename);
	void MakeSpriteSheetUnique(int sheetID);
	size_t FindOrAddSpriteSheet(const char* sheetFilename);
	void ReserveAnimationCount(int count);
	void AddAnimation(const char* name, int speed, int frameToLoop);
	void AddAnimation(const char* name, int speed, int frameToLoop, int frmAlloc);
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
	void RemoveFrame(int animID, int frameID);
	void RemoveAllFrames(int animID);
	void RefreshGraphicsID();
	void UpdateFrame(int animID, int frameID);
	void ConvertToRGBA();
	void ConvertToPalette(unsigned paletteNumber);
	static bool IsFile(Stream* stream);
	bool LoadAnimation(const char* filename);
	int FindAnimation(const char* animname);
	void LinkAnimation(vector<Animation> ani);
	bool SaveAnimation(const char* filename);
	void Unload();
	~ISprite();
};

#endif /* ENGINE_RESOURCETYPES_ISPRITE_H */
