#ifndef ENGINE_SCENE_TILESPRITEINFO_H
#define ENGINE_SCENE_TILESPRITEINFO_H

class TileSpriteInfo {
public:
	ISprite* Sprite;
	int AnimationIndex;
	int FrameIndex;
	size_t TilesetID;
};

#endif /* ENGINE_SCENE_TILESPRITEINFO_H */
