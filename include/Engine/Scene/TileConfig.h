#ifndef ENGINE_SCENE_TILECONFIG_H
#define ENGINE_SCENE_TILECONFIG_H

#include "Engine/Includes/Standard.h"

class TileConfig {
public:
	Uint8 CollisionTop[16];
	Uint8 CollisionLeft[16];
	Uint8 CollisionRight[16];
	Uint8 CollisionBottom[16];
	Uint8 AngleTop;
	Uint8 AngleLeft;
	Uint8 AngleRight;
	Uint8 AngleBottom;
	Uint8 Behavior;
	Uint8 IsCeiling;
};

#endif /* ENGINE_SCENE_TILECONFIG_H */
