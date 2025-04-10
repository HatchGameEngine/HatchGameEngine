#ifndef ENGINE_SCENE_TILESPRITEINFO_H
#define ENGINE_SCENE_TILESPRITEINFO_H

#include "Engine/ResourceTypes/ISprite.h"

class TileSpriteInfo {
public:
	ISprite* Sprite;
	int AnimationIndex;
	int FrameIndex;
	size_t TilesetID;
};

#endif /* ENGINE_SCENE_TILESPRITEINFO_H */
