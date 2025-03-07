#ifndef ENGINE_TYPES_TILESET_H
#define ENGINE_TYPES_TILESET_H

#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/Scene/TileAnimation.h>

class Tileset {
public:
	ISprite* Sprite = nullptr;
	char* Filename = nullptr;
	int NumCols = 0;
	int NumRows = 0;
	int TileWidth = 0;
	int TileHeight = 0;
	size_t StartTile = 0;
	size_t FirstGlobalTileID = 0;
	size_t TileCount = 0;
	unsigned PaletteID = 0;
	std::map<int, TileAnimator> AnimatorMap;

	Tileset(ISprite* sprite,
		int tileWidth,
		int tileHeight,
		size_t firstgid,
		size_t startTile,
		size_t tileCount,
		char* filename);
	void RunAnimations();
	void RestartAnimations();
	void AddTileAnimSequence(int tileID,
		TileSpriteInfo* tileSpriteInfo,
		vector<int>& tileIDs,
		vector<int>& durations);
	void AddTileAnimSequence(int tileID,
		TileSpriteInfo* tileSpriteInfo,
		ISprite* animSprite,
		int animID);
	TileAnimator* GetTileAnimSequence(int tileID);
};

#endif /* ENGINE_TYPES_TILESET_H */
