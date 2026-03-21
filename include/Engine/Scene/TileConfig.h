#ifndef ENGINE_SCENE_TILECONFIG_H
#define ENGINE_SCENE_TILECONFIG_H

#include <Engine/Scene/SceneEnums.h>

class TileConfig {
public:
	Uint8 CollisionTop[TILE_SIZE_MAX];
	Uint8 CollisionLeft[TILE_SIZE_MAX];
	Uint8 CollisionRight[TILE_SIZE_MAX];
	Uint8 CollisionBottom[TILE_SIZE_MAX];
	Uint8 AngleTop;
	Uint8 AngleLeft;
	Uint8 AngleRight;
	Uint8 AngleBottom;
	Uint8 Behavior;
	Uint8 IsCeiling;
};

#endif /* ENGINE_SCENE_TILECONFIG_H */
