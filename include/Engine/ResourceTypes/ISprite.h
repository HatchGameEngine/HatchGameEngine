#ifndef ENGINE_RESOURCETYPES_ISPRITE_H
#define ENGINE_RESOURCETYPES_ISPRITE_H

#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/Resourceable.h>
#include <Engine/Sprites/Animation.h>

class ISprite : public Resourceable {
public:
	vector<Texture*> Spritesheets;
	vector<string> SpritesheetFilenames;
	int CollisionBoxCount = 0;
	vector<Animation> Animations;
	int ID = 0;
	int FrameCount = 0;

	ISprite();
	ISprite(const char* filename);
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
