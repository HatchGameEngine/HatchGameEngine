#ifndef ENGINE_SCENE_TILESPRITEINFO_H
#define ENGINE_SCENE_TILESPRITEINFO_H

struct TileSpriteInfo {
	ISprite* Sprite = nullptr;
	int AnimationIndex = 0;
	int FrameIndex = 0;
	size_t TilesetID = 0;
	bool IsAnimated = false;
};

#endif /* ENGINE_SCENE_TILESPRITEINFO_H */
